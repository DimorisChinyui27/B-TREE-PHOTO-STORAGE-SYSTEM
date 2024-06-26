// REMEMBER DIMORIS
//  TO BUILD THE PROJECT FILES TO USE THE MSYS64 compiler open command pallete using ctrl + shift + P, search and choose CMAKE:Delete Cache  and Reconfigure
//  TO BUILD THE executable with cmake tools """cmake --build build"""
#define OPENSSL_SUPPRESS_DEPRECATED // To fix deprecated openssl error
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <openssl/sha.h>
#include <zlib.h>

using namespace std;
namespace fs = std::filesystem;

// Struct to represent a photo
struct Photo
{
    string fileName;
    string hash;
    string dateAdded;
    vector<unsigned char> compressedData;
};

// B+ Tree class
class BPlusTree
{
private:
    int order;
    struct Node
    {
        bool isLeaf;
        vector<string> keys;
        vector<Photo> photos;
        vector<Node *> children;
        Node *next;

        Node(bool leaf, int ord)
        {
            isLeaf = leaf;
            next = nullptr;
            keys.reserve(ord);
            photos.reserve(ord);
            children.reserve(ord + 1);
        }
    };

    Node *root;

    void insertInternal(string key, Photo photo, Node *node, Node *child);
    void removeInternal(string key, Node *node, Node *child);
    Node *findParent(Node *node, Node *child);

public:
    BPlusTree(int ord)
    {
        order = ord;
        root = nullptr;
    }

    void insert(string key, Photo photo);
    void remove(string key);
    Photo *find(string key);
    void display();
};

// ProgressBar class to manage and display the progress of file processing
class ProgressBar
{
private:
    int total;
    int step;
    int current;
    int barWidth;
    char symbol;

public:
    // Constructor initializes the progress bar's total count, width, and fill symbol
    ProgressBar(int total, int width, char symbol = '#') : total(total), barWidth(width), symbol(symbol), current(0)
    {
        step = 0;
    }

    // Update the progress bar's current value and calculate the step for the visual representation
    void update(int value)
    {
        current += value;
        step = (current * barWidth) / total;
    }

    // Display the current progress bar status in the console
    void display() const
    {
        cout << "[";
        int pos = 0;
        for (; pos < step; ++pos)
        {
            cout << symbol;
        }
        for (; pos < barWidth; ++pos)
        {
            cout << " ";
        }
        cout << "] " << int((current * 100.0) / total) << " %\r";
        cout.flush();
    }

    // Clean up the display when the progress is complete
    void done()
    {
        cout << endl;
    }
};

// Function to generate SHA-512 hash of a file
string generateHash(const string &fileName)
{
    ifstream file(fileName, ios::binary);
    if (!file)
    {
        return "";
    }

    unsigned char hash[SHA512_DIGEST_LENGTH];
    SHA512_CTX sha512;
    SHA512_Init(&sha512);
    char buffer[4096];
    while (file.read(buffer, sizeof(buffer)).gcount() > 0)
    {
        SHA512_Update(&sha512, buffer, file.gcount());
    }
    SHA512_Final(hash, &sha512);

    stringstream ss;
    for (int i = 0; i < SHA512_DIGEST_LENGTH; i++)
    {
        ss << hex << setw(2) << setfill('0') << static_cast<int>(hash[i]);
    }
    return ss.str();
}

// Function to get current date in YYYY-MM-DD format
string getCurrentDate()
{
    time_t now = time(nullptr);
    tm *localTime = localtime(&now);
    char buffer[11];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d", localTime);
    return string(buffer);
}

// Function to compress a photo file
vector<unsigned char> compressPhoto(const string &fileName)
{
    ifstream file(fileName, ios::binary);
    if (!file)
    {
        return {};
    }

    vector<unsigned char> compressedData;
    const int CHUNK_SIZE = 4096;
    unsigned char buffer[CHUNK_SIZE];
    z_stream stream;
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;
    deflateInit(&stream, Z_DEFAULT_COMPRESSION);

    while (file.read(reinterpret_cast<char *>(buffer), CHUNK_SIZE))
    {
        stream.avail_in = file.gcount();
        stream.next_in = buffer;
        do
        {
            unsigned char outBuffer[CHUNK_SIZE];
            stream.avail_out = CHUNK_SIZE;
            stream.next_out = outBuffer;
            deflate(&stream, Z_NO_FLUSH);
            compressedData.insert(compressedData.end(), outBuffer, outBuffer + CHUNK_SIZE - stream.avail_out);
        } while (stream.avail_out == 0);
    }

    stream.avail_in = file.gcount();
    stream.next_in = buffer;
    unsigned char outBuffer[CHUNK_SIZE];
    do
    {
        stream.avail_out = CHUNK_SIZE;
        stream.next_out = outBuffer;
        deflate(&stream, Z_FINISH);
        compressedData.insert(compressedData.end(), outBuffer, outBuffer + CHUNK_SIZE - stream.avail_out);
    } while (stream.avail_out == 0);

    deflateEnd(&stream);
    return compressedData;
}

// Function to decompress a compressed photo
void decompressPhoto(const vector<unsigned char> &compressedData,
                     const string &fileName)
{
    ofstream file(fileName, ios::binary);
    if (!file)
    {
        return;
    }

    const int CHUNK_SIZE = 4096;
    unsigned char buffer[CHUNK_SIZE];
    z_stream stream;
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;
    inflateInit(&stream);

    stream.avail_in = compressedData.size();
    stream.next_in = const_cast<unsigned char *>(compressedData.data());

    do
    {
        stream.avail_out = CHUNK_SIZE;
        stream.next_out = buffer;
        inflate(&stream, Z_NO_FLUSH);
        file.write(reinterpret_cast<char *>(buffer), CHUNK_SIZE - stream.avail_out);
    } while (stream.avail_out == 0);

    inflateEnd(&stream);
}

// Function to insert a photo into the B+ tree
void BPlusTree::insert(string key, Photo photo)
{
    if (root == nullptr)
    {
        root = new Node(true, order);
        root->keys.push_back(key);
        root->photos.push_back(photo);
    }
    else
    {
        Node *node = root;
        while (!node->isLeaf)
        {
            for (int i = 0; i < node->keys.size(); i++)
            {
                if (key < node->keys[i])
                {
                    node = node->children[i];
                    break;
                }
                if (i == node->keys.size() - 1)
                {
                    node = node->children[i + 1];
                    break;
                }
            }
        }

        if (node->keys.size() < order - 1)
        {
            int i = 0;
            while (i < node->keys.size() && key > node->keys[i])
            {
                i++;
            }
            node->keys.insert(node->keys.begin() + i, key);
            node->photos.insert(node->photos.begin() + i, photo);
        }
        else
        {
            Node *newLeaf = new Node(true, order);
            vector<string> tempKeys = node->keys;
            vector<Photo> tempPhotos = node->photos;
            tempKeys.push_back(key);
            tempPhotos.push_back(photo);

            node->keys.clear();
            node->photos.clear();

            int i = 0;
            while (i < order / 2)
            {
                node->photos.push_back(tempPhotos[i]);
                node->keys.push_back(tempKeys[i]);
                i++;
            }

            while (i < tempKeys.size())
            {
                newLeaf->photos.push_back(tempPhotos[i]);
                newLeaf->keys.push_back(tempKeys[i]);
                i++;
            }

            newLeaf->next = node->next;
            node->next = newLeaf;

            string newKey = newLeaf->keys[0];
            insertInternal(newKey, newLeaf->photos[0], root, newLeaf);
        }
    }
}

// Function to insert a key into the internal nodes of the B+ tree
void BPlusTree::insertInternal(string key, Photo photo, Node *node, Node *child)
{
    if (node->keys.size() < order - 1)
    {
        int i = 0;
        while (i < node->keys.size() && key > node->keys[i])
        {
            i++;
        }
        node->keys.insert(node->keys.begin() + i, key);
        node->photos.insert(node->photos.begin() + i, photo);
        node->children.insert(node->children.begin() + i + 1, child);
    }
    else
    {
        Node *newInternal = new Node(false, order);
        vector<string> tempKeys = node->keys;
        vector<Photo> tempPhotos = node->photos;
        vector<Node *> tempChildren = node->children;
        tempKeys.push_back(key);
        tempPhotos.push_back(photo);
        tempChildren.push_back(child);

        node->keys.clear();
        node->photos.clear();
        node->children.clear();

        int i = 0;
        while (i < (order + 1) / 2 - 1)
        {
            node->photos.push_back(tempPhotos[i]);
            node->keys.push_back(tempKeys[i]);
            node->children.push_back(tempChildren[i]);
            i++;
        }
        node->children.push_back(tempChildren[i]);
        string newParentKey = tempKeys[i];
        Photo newParentPhoto = tempPhotos[i];
        i++;

        while (i < tempKeys.size())
        {
            newInternal->photos.push_back(tempPhotos[i]);
            newInternal->keys.push_back(tempKeys[i]);
            newInternal->children.push_back(tempChildren[i]);
            i++;
        }
        newInternal->children.push_back(tempChildren[i]);

        if (node == root)
        {
            Node *newRoot = new Node(false, order);
            newRoot->keys.push_back(newParentKey);
            newRoot->photos.push_back(newParentPhoto);
            newRoot->children.push_back(node);
            newRoot->children.push_back(newInternal);
            root = newRoot;
        }
        else
        {
            insertInternal(newParentKey, newParentPhoto, findParent(root, node), newInternal);
        }
    }
}

// Function to remove a key from the B+ tree
void BPlusTree::remove(string key)
{
    if (root == nullptr)
    {
        cout << "The tree is empty\n";
    }
    else
    {
        Node *node = root;
        Node *parent = nullptr;
        int leftSibling = -1, rightSibling = -1;

        while (!node->isLeaf)
        {
            for (int i = 0; i < node->keys.size(); i++)
            {
                parent = node;
                leftSibling = i - 1;
                rightSibling = i + 1;
                if (key < node->keys[i])
                {
                    node = node->children[i];
                    break;
                }
                if (i == node->keys.size() - 1)
                {
                    leftSibling = i;
                    rightSibling = -1;
                    node = node->children[i + 1];
                    break;
                }
            }
        }

        bool found = false;
        int pos;
        for (pos = 0; pos < node->keys.size(); pos++)
        {
            if (node->keys[pos] == key)
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            cout << "The key " << key << " was not found in the tree\n";
            return;
        }

        node->keys.erase(node->keys.begin() + pos);
        node->photos.erase(node->photos.begin() + pos);

        if (node == root)
        {
            if (node->keys.size() == 0)
            {
                cout << "The tree is empty\n";
                delete node;
                root = nullptr;
            }
            return;
        }

        if (node->keys.size() >= (order + 1) / 2 - 1)
        {
            return;
        }

        if (leftSibling >= 0)
        {
            Node *leftNode = parent->children[leftSibling];
            if (leftNode->keys.size() >= (order + 1) / 2)
            {
                node->keys.insert(node->keys.begin(), parent->keys[leftSibling]);
                node->photos.insert(node->photos.begin(), parent->photos[leftSibling]);
                parent->keys[leftSibling] = leftNode->keys.back();
                parent->photos[leftSibling] = leftNode->photos.back();
                leftNode->keys.pop_back();
                leftNode->photos.pop_back();
                return;
            }
        }

        if (rightSibling < parent->children.size())
        {
            Node *rightNode = parent->children[rightSibling];
            if (rightNode->keys.size() >= (order + 1) / 2)
            {
                node->keys.push_back(parent->keys[pos]);
                node->photos.push_back(parent->photos[pos]);
                parent->keys[pos] = rightNode->keys.front();
                parent->photos[pos] = rightNode->photos.front();
                rightNode->keys.erase(rightNode->keys.begin());
                rightNode->photos.erase(rightNode->photos.begin());
                return;
            }
        }

        if (leftSibling >= 0)
        {
            Node *leftNode = parent->children[leftSibling];
            leftNode->keys.insert(leftNode->keys.end(), node->keys.begin(), node->keys.end());
            leftNode->photos.insert(leftNode->photos.end(), node->photos.begin(), node->photos.end());
            leftNode->next = node->next;
            parent->keys.erase(parent->keys.begin() + leftSibling);
            parent->photos.erase(parent->photos.begin() + leftSibling);
            parent->children.erase(parent->children.begin() + leftSibling + 1);
            delete node;
        }
        else
        {
            Node *rightNode = parent->children[rightSibling];
            node->keys.insert(node->keys.end(), rightNode->keys.begin(), rightNode->keys.end());
            node->photos.insert(node->photos.end(), rightNode->photos.begin(), rightNode->photos.end());
            node->next = rightNode->next;
            parent->keys.erase(parent->keys.begin() + rightSibling - 1);
            parent->photos.erase(parent->photos.begin() + rightSibling - 1);
            parent->children.erase(parent->children.begin() + rightSibling);
            delete rightNode;
        }

        if (parent == root && parent->keys.size() == 0)
        {
            root = parent->children.front();
            delete parent;
            return;
        }

        if (parent->keys.size() < (order + 1) / 2 - 1)
        {
            removeInternal(parent->keys[0], root, parent);
        }
    }
}

// Function to remove a key from the internal nodes of the B+ tree
void BPlusTree::removeInternal(string key, Node *node, Node *child)
{
    if (node == root)
    {
        if (node->keys.size() == 1)
        {
            if (node->children[1] == child)
            {
                delete child;
                root = node->children[0];
                delete node;
                return;
            }
            else if (node->children[0] == child)
            {
                delete child;
                root = node->children[1];
                delete node;
                return;
            }
        }
    }

    int pos;
    for (pos = 0; pos < node->keys.size(); pos++)
    {
        if (node->keys[pos] == key)
        {
            break;
        }
    }

    for (int i = pos; i < node->keys.size() - 1; i++)
    {
        node->keys[i] = node->keys[i + 1];
        node->photos[i] = node->photos[i + 1];
    }
    node->keys.pop_back();
    node->photos.pop_back();

    for (int i = 0; i < node->children.size(); i++)
    {
        if (node->children[i] == child)
        {
            node->children.erase(node->children.begin() + i);
            break;
        }
    }

    if (node->keys.size() >= (order + 1) / 2 - 1)
    {
        return;
    }

    if (node == root)
    {
        return;
    }

    Node *parent = findParent(root, node);

    int leftSibling = -1, rightSibling = -1;
    for (int i = 0; i < parent->children.size(); i++)
    {
        if (parent->children[i] == node)
        {
            leftSibling = i - 1;
            rightSibling = i + 1;
            break;
        }
    }

    Node *leftNode = nullptr;
    Node *rightNode = nullptr;

    if (leftSibling >= 0)
    {
        leftNode = parent->children[leftSibling];
        if (leftNode->keys.size() >= (order + 1) / 2)
        {
            node->keys.insert(node->keys.begin(), parent->keys[leftSibling]);
            node->photos.insert(node->photos.begin(), parent->photos[leftSibling]);
            parent->keys[leftSibling] = leftNode->keys.back();
            parent->photos[leftSibling] = leftNode->photos.back();
            leftNode->keys.pop_back();
            leftNode->photos.pop_back();
            node->children.insert(node->children.begin(), leftNode->children.back());
            leftNode->children.pop_back();
            return;
        }
    }

    if (rightSibling < parent->children.size())
    {
        rightNode = parent->children[rightSibling];
        if (rightNode->keys.size() >= (order + 1) / 2)
        {
            node->keys.push_back(parent->keys[rightSibling - 1]);
            node->photos.push_back(parent->photos[rightSibling - 1]);
            parent->keys[rightSibling - 1] = rightNode->keys.front();
            parent->photos[rightSibling - 1] = rightNode->photos.front();
            rightNode->keys.erase(rightNode->keys.begin());
            rightNode->photos.erase(rightNode->photos.begin());
            node->children.push_back(rightNode->children.front());
            rightNode->children.erase(rightNode->children.begin());
            return;
        }
    }

    if (leftNode != nullptr)
    {
        leftNode->keys.push_back(parent->keys[leftSibling]);
        leftNode->photos.push_back(parent->photos[leftSibling]);
        leftNode->keys.insert(leftNode->keys.end(), node->keys.begin(), node->keys.end());
        leftNode->photos.insert(leftNode->photos.end(), node->photos.begin(), node->photos.end());
        leftNode->children.insert(leftNode->children.end(), node->children.begin(), node->children.end());
        parent->keys.erase(parent->keys.begin() + leftSibling);
        parent->photos.erase(parent->photos.begin() + leftSibling);
        parent->children.erase(parent->children.begin() + leftSibling + 1);
        delete node;
    }
    else if (rightNode != nullptr)
    {
        node->keys.push_back(parent->keys[rightSibling - 1]);
        node->photos.push_back(parent->photos[rightSibling - 1]);
        node->keys.insert(node->keys.end(), rightNode->keys.begin(), rightNode->keys.end());
        node->photos.insert(node->photos.end(), rightNode->photos.begin(), rightNode->photos.end());
        node->children.insert(node->children.end(), rightNode->children.begin(), rightNode->children.end());
        parent->keys.erase(parent->keys.begin() + rightSibling - 1);
        parent->photos.erase(parent->photos.begin() + rightSibling - 1);
        parent->children.erase(parent->children.begin() + rightSibling);
        delete rightNode;
    }

    if (parent->keys.size() < (order + 1) / 2 - 1)
    {
        if (parent == root && parent->keys.size() == 0)
        {
            root = parent->children.front();
            delete parent;
        }
        else
        {
            removeInternal(parent->keys[0], root, parent);
        }
    }
}

// Function to find the parent of a node
BPlusTree::Node *BPlusTree::findParent(Node *node, Node *child)
{
    Node *parent = nullptr;
    if (node->isLeaf || (node->children[0])->isLeaf)
    {
        return nullptr;
    }

    for (int i = 0; i < node->children.size(); i++)
    {
        if (node->children[i] == child)
        {
            parent = node;
            return parent;
        }
        else
        {
            parent = findParent(node->children[i], child);
            if (parent != nullptr)
            {
                return parent;
            }
        }
    }

    return nullptr;
}

// Function to find a photo in the B+ tree
Photo *BPlusTree::find(string key)
{
    if (root == nullptr)
    {
        return nullptr;
    }
    else
    {
        Node *node = root;
        while (!node->isLeaf)
        {
            // Traverse down the tree to reach a leaf node
            for (int i = 0; i < node->keys.size(); i++)
            {
                if (key < node->keys[i])
                {
                    node = node->children[i];
                    break;
                }
                if (i == node->keys.size() - 1)
                {
                    node = node->children[i + 1];
                    break;
                }
            }
        }

        // Iterate through the leaf nodes
        while (node != nullptr)
        {
            for (int i = 0; i < node->keys.size(); i++)
            {
                if (key == node->keys[i])
                {
                    return &(node->photos[i]); // Found the photo!
                }
            }
            node = node->next; // Move to the next leaf node
        }

        return nullptr; // Photo not found
    }
}

// Function to display the keys in the leaf nodes of the B+ tree
void BPlusTree::display()
{
    if (root == nullptr)
    {
        cout << "The tree is empty\n";
    }
    else
    {
        Node *node = root;
        while (!node->isLeaf)
        {
            node = node->children.front();
        }

        while (node != nullptr)
        {
            for (int i = 0; i < node->keys.size(); i++)
            {
                cout << node->keys[i] << " ";
            }
            cout << "| ";
            node = node->next;
        }
        cout << endl;
    }
}

int main()
{
    BPlusTree bpt(3); // Create a B+ tree with order 3
    string photoDirectory = "C:\\Users\\Utilisateur\\Downloads\\100000PHOTOS\\archive";
    int numPhotos = 0;

    // Vector to hold paths of photo files
    vector<string> allFiles;

    // Check the photo directory and gather all photo files
    for (const auto &entry : fs::directory_iterator(photoDirectory))
    {
        if (entry.is_regular_file())
        {
            string extension = entry.path().extension().string();
            if (extension == ".jpg" || extension == ".png")
            {
                allFiles.push_back(entry.path().string());
            }
        }
    }
    int totalFiles = allFiles.size();
    if (totalFiles == 0)
    {
        // Inform the user if the photo directory is empty
        cout << "The photo directory is empty. No photos to add." << endl;
        return 0; // Exit if no photos to process
    }

    // Initialize the progress bar with the total number of photo files and a width of 70 characters
    ProgressBar progressBar(totalFiles, 70);

    // Process each photo file
    for (const auto &fileName : allFiles)
    {
        string hash = generateHash(fileName);
        string dateAdded = getCurrentDate();
        vector<unsigned char> compressedData = compressPhoto(fileName);
        Photo photo{fileName, hash, dateAdded, compressedData};
        bpt.insert(hash, photo);
        numPhotos++;

        // Update and display the progress bar after processing each photo
        progressBar.update(1);
        progressBar.display();

        // Break out of the loop after processing over 100000 photos
        if (numPhotos == 100000)
        {
            break;
        }
    }

    // Mark the progress as complete
    progressBar.done();
    cout << "Number of photos added: " << numPhotos << endl;

    int choice;
    string photoHash; // Store photo hash instead of name
    while (true)
    {
        cout << "\nOptions:" << endl;
        cout << "1. Display the tree" << endl;
        cout << "2. Find a photo by hash and print out the information" << endl;
        cout << "3. Remove a photo by hash, print out the information, and display the tree" << endl;
        cout << "4. Terminate the program" << endl;
        cout << "Enter your choice (1-4): ";
        cin >> choice;

        switch (choice)
        {
        case 1:
            bpt.display();
            break;
        case 2:
            cout << "Enter the hash of the photo: ";
            cin >> photoHash;
            {
                Photo *foundPhoto = bpt.find(photoHash);
                if (foundPhoto != nullptr)
                {
                    cout << "Found photo:" << endl;
                    cout << "File Name: " << foundPhoto->fileName << endl;
                    cout << "Hash: " << foundPhoto->hash << endl;
                    cout << "Date Added: " << foundPhoto->dateAdded << endl;
                    string extension = fs::path(foundPhoto->fileName).extension().string();
                    string decompressedFileName = "decompressed_" + fs::path(foundPhoto->fileName).stem().string() + extension;
                    decompressPhoto(foundPhoto->compressedData, decompressedFileName);
                    cout << "Decompressed photo saved as: " << decompressedFileName << endl;
                }
                else
                {
                    cout << "Photo not found" << endl;
                }
            }
            break;
        case 3:
            cout << "Enter the hash of the photo: ";
            cin >> photoHash;
            {
                Photo *foundPhoto = bpt.find(photoHash);
                if (foundPhoto != nullptr)
                {
                    cout << "Found photo:" << endl;
                    cout << "File Name: " << foundPhoto->fileName << endl;
                    cout << "Hash: " << foundPhoto->hash << endl;
                    cout << "Date Added: " << foundPhoto->dateAdded << endl;
                    string extension = fs::path(foundPhoto->fileName).extension().string();
                    string decompressedFileName = "decompressed_" + fs::path(foundPhoto->fileName).stem().string() + extension;
                    decompressPhoto(foundPhoto->compressedData, decompressedFileName);
                    cout << "Decompressed photo saved as: " << decompressedFileName << endl;
                    cout << "Removing photo: " << foundPhoto->fileName << endl;
                    bpt.remove(photoHash);
                    cout << "Photo removed" << endl;
                }
                else
                {
                    cout << "Photo not found" << endl;
                }
            }
            break;
        case 4:
            cout << "Terminating the program. Goodbye!" << endl;
            return 0;
        default:
            cout << "Invalid choice. Please try again." << endl;
        }
    }
    return 0;
}