#include <iostream>
#include <fstream>
#include <sys/stat.h> //for mkdir()
#include <string>
#include <openssl/sha.h>
#include <iomanip>
#include <zlib.h>
#include <filesystem>
#include <unordered_set>
#include <vector>

using namespace std;
void create_dir(const string& dir_name){
    struct stat info;
    if (stat(dir_name.c_str(), &info) != 0){//=-1
        //directory dir__name doesn't exists
        int dir_result=mkdir(dir_name.c_str(),0755);
        if(dir_result==0)
            cout<<"Directory "<<dir_name<<" created successfully"<<endl;
        else if(dir_result==-1)
            cout<<"Failed to create directory "<<dir_name<<endl;
    }
    else{
        cout<<"Directory "<<dir_name<<" already exists"<<endl;
        return;
    }
    
}
void create_file(const string& file_name,const string& file_content=""){
    ofstream new_file(file_name);
    if(new_file.is_open()){
        new_file << file_content;
        new_file.close();
        cout<<"File "<<file_name<<" is created successfully"<<endl;
    }
    else{
        cout<<"Error while creating file "<<file_name;
        return;
    }
}

string compute_sha_hash(const string& file_content){
    //calculating sha1 hash
    unsigned char sha1_val[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(file_content.data()), file_content.size(), sha1_val);

    ostringstream hash;
    for(int i=0;i<SHA_DIGEST_LENGTH;i++)
        hash<<setw(2)<<setfill('0')<<hex << static_cast<int>(sha1_val[i]);

    return hash.str();
}

void write_as_blob(string& sha1_val,const string& file_name){
    string blob_dir = ".mygit/objects/" + sha1_val.substr(0, 2); // Git-style subdirectory
    filesystem::create_directories(blob_dir);
    string blob_path = blob_dir + "/" + sha1_val.substr(2); // Git-style file name
    if (filesystem::exists(blob_path)) {
        cout << "Blob object already exists for " << file_name << ", skipping write." << endl;
        return;
    }
//read the original file
ifstream og_file(file_name,ios::binary);

//read the file into a string
ostringstream out_ss;
out_ss << og_file.rdbuf();
string og_file_content=out_ss.str();

    //to identify its type as blob/tree in future
string obj_type="blob ";
og_file_content=obj_type+og_file_content;   

    //compress the contents
    uLongf compressed_sz=compressBound(og_file_content.size());
    vector<Bytef>comp(compressed_sz);

    int compress_result=compress(comp.data(),&compressed_sz, 
                                reinterpret_cast<const Bytef*>(og_file_content.data()),og_file_content.size());
                    
    if (compress_result != Z_OK){
        cout<<"Compression failed with error code: "<<to_string(compress_result)<<endl;
        return;
    }
    // Write the compressed data to a blob file
    // create_dir("objects/"+sha1_val.substr(0,2));
    ofstream blob_obj(blob_path, ios::binary);
    blob_obj.write(reinterpret_cast<const char*>(comp.data()),compressed_sz);
    cout<<"Blob object written to "<<blob_path<<endl;
}
void write_as_tree(string& sha1_val, const string& tree_content) {
    // Ensure .mygit/objects directory exists
    string tree_dir = ".mygit/objects/"+sha1_val.substr(0, 2);
    filesystem::create_directories(tree_dir);

    // Determine the path where the compressed object will be stored
    string tree_path = tree_dir + "/"+sha1_val.substr(2);
    
    // If the tree object already exists, skip writing
    if (filesystem::exists(tree_path)) {
        cout << "Tree object already exists, skipping write." << endl;
        return;
    }

    // Prepare the full tree content with the "tree " header
    string full_content = "tree " + tree_content;

    // Compress the content
    uLongf compressed_size = compressBound(full_content.size());
    vector<Bytef> compressed_data(compressed_size);
    int compress_result = compress(compressed_data.data(), &compressed_size, 
                                   reinterpret_cast<const Bytef*>(full_content.data()), full_content.size());

    if (compress_result != Z_OK) {
        cerr << "Failed to compress tree content, error code: " << compress_result << endl;
        return;
    }

    // Write compressed content to the tree directory
    ofstream tree_obj(tree_path, ios::binary);
    if (!tree_obj.is_open()) {
        cerr << "Failed to open file for writing: " << tree_path << endl;
        return;
    }

    tree_obj.write(reinterpret_cast<const char*>(compressed_data.data()), compressed_size);
    tree_obj.close();
    cout << "Compressed tree object written to " << tree_path << endl;
}

void cat_file_func(string& flag, string& sha_val){
    string obj_path=".mygit/objects/"+sha_val.substr(0,2)+"/"+sha_val.substr(2);
    if(!filesystem::exists(obj_path)){
        cout<<"Object with sha:"<<sha_val<<" does not exist"<<endl;
        return;
    }
    //open the object and decompress it
    ifstream obj_file(obj_path,ios::binary);
    vector<char> comp_data((istreambuf_iterator<char>(obj_file)),istreambuf_iterator<char>());

    uLongf og_size=comp_data.size()*2;
    vector<Bytef> og_data(og_size);

    //adjysut the size until it can hold original data
    while(uncompress(og_data.data(),&og_size,reinterpret_cast<const Bytef*>(comp_data.data()),comp_data.size())==Z_BUF_ERROR){
        og_size<<1;
        og_data.resize(og_size);
    }
    string og_content(og_data.begin(),og_data.begin()+og_size);
    size_t pos = og_content.find(' ');
    string header = og_content.substr(0, pos);
    string file_content = og_content.substr(pos + 1);
    //check the flags
    if(flag=="-p"){
        cout<<file_content<<endl;
    }
    else if(flag=="-s"){
        cout<<"Size: "<<file_content.size()<<" bytes"<<endl;
    }
    else if(flag=="-t"){
    // string type=og_content.substr(0,og_content.find(" "));
        cout<<"Type: "<<header<<endl;
    }
}

string my_write_tree(const string &curr_dir) {
    string dir_permission="40000 ";
    string file_permission="100644 ";
    ostringstream tree_content;
    for (const auto &curr_entry : filesystem::directory_iterator(curr_dir)) {
        // Skip the .mygit directory and other specified files
        if (curr_entry.path().filename() == ".mygit" 
            || curr_entry.path().filename() == "main.cpp" 
            || curr_entry.path().filename() == "mygit"
            || curr_entry.path().filename()==".vscode") continue;

        if (curr_entry.is_directory()) {
            // Process subdirectories recursively
            string sub_dir_hash = my_write_tree(curr_entry.path().string());
            tree_content<<dir_permission<<"tree "<< sub_dir_hash << " " << curr_entry.path().filename().string() << endl;
        } 
        else if (curr_entry.is_regular_file()) {
            // Process regular files
            ifstream file(curr_entry.path(), ios::binary);
            if (!file.is_open()) {
                cerr << "Unable to open file " << curr_entry.path() << " while traversing in write-tree function" << endl;
                continue; // Skip this file and continue with the next one
            }     
            ostringstream out_ss;
            out_ss << file.rdbuf();
            string file_content = out_ss.str();
            file.close();

            string file_hash = compute_sha_hash(file_content);       
        
           // string file_hash = compute_sha_hash(curr_entry.path());
            write_as_blob(file_hash, curr_entry.path());
            tree_content<<file_permission<<"blob " << file_hash << " " << curr_entry.path().filename().string() << endl;
        }
    }
    
    // Write the tree object with a "tree" header
    string tree_data =tree_content.str();
    string tree_sha1_val = compute_sha_hash(tree_data);
    cout<<"TREE DATA:"<<endl<<tree_data<<endl;
    write_as_tree(tree_sha1_val, tree_content.str());

   // cout<<"Tree sha: "<<tree_sha1_val<<endl;
    return tree_sha1_val;
}
bool is_file_in_index(const string& file_hash) {
    ifstream index_file(".mygit/index");
    if (!index_file.is_open()) {
        cerr << "Unable to open index file" << endl;
        return false;
    }

    string line;
    while (getline(index_file, line)) {
        if (line.find(file_hash) != string::npos) {
            return true; // File is already in index
        }
    }
    return false; // File not found in index
}
void add_to_index(const string& file_name, const string& sha1_val) {
    string file_permission="100644";
    ofstream index_file(".mygit/index", ios::app);
    if (index_file.is_open()) {
        index_file << file_permission<<" "<<sha1_val << " " << file_name << endl;
        index_file.close();
        cout << "Added " << file_name << " to staging area" << endl;
    } else {
        cerr << "Error opening index file" << endl;
    }
}
void process_file(const string& file_name) {
    ifstream file(file_name, ios::binary);
    if (!file.is_open()) {
        cerr << "Unable to open file " << file_name << endl;
        return;
    }
    ostringstream out_ss;
    out_ss << file.rdbuf();
    string file_content = out_ss.str();
    file.close();

    string sha1_val = compute_sha_hash(file_content);
    string blob_path=".mygit/objects/"+sha1_val.substr(0,2)+"/"+sha1_val.substr(2);
    if (!filesystem::exists(blob_path)) {
        cout <<"Wite this file using hash-object command before adding to staging area"<<endl;
        return;
    }
     // Check if the file is already in the index
    if (is_file_in_index(sha1_val)) {
        cout << file_name <<"with"<<sha1_val<< " is already in the staging area" << endl;
        return; // Skip this file if it's already staged
    }
    //write_as_blob(sha1_val, file_content);
    add_to_index(file_name, sha1_val);
}

void add_files(const vector<string>& objs) {
    for (const auto& entry : objs) {
        filesystem::path entry_path(entry);

        if (filesystem::is_directory(entry_path)) {
            // Recursively add files in the directory
            for (const auto& sub_entry : filesystem::recursive_directory_iterator(entry_path)) {
                if (sub_entry.is_regular_file()) {
                    process_file(sub_entry.path().string());
                }
            }
        } else if (filesystem::is_regular_file(entry_path)) {
            // Add individual file
            process_file(entry_path.string());
        }
    }
}

void add_all_files() {
    vector<string> files;
    for (const auto& entry : filesystem::directory_iterator(filesystem::current_path())) {
        if (entry.is_regular_file() 
            && entry.path().filename() != ".mygit" 
            && entry.path().filename() != "mygit" 
            && entry.path().filename() != "main.cpp"
            && entry.path().filename() != ".vscode") {
            files.push_back(entry.path().string());
        } else if (entry.is_directory()) {
            files.push_back(entry.path().string());
        }
    }
    add_files(files);
}

void ls_tree(const string& tree_sha, bool name_only) {
    cout << "Tree SHA: " << tree_sha << endl;
    string obj_path = ".mygit/objects/" + tree_sha.substr(0, 2)+"/"+tree_sha.substr(2);

    if (!filesystem::exists(obj_path)) {
        cout << "Object with SHA: " << tree_sha << " does not exist" << endl;
        return;
    }

    // Open the object file in binary mode and read its compressed content
    ifstream obj_file(obj_path, ios::binary);
    if (!obj_file.is_open()) {
        cerr << "Failed to open object file: " << obj_path << endl;
        return;
    }

    // Read the compressed data from the file
    stringstream buffer;
    buffer << obj_file.rdbuf();
    string compressed_content = buffer.str();
    obj_file.close();

    // Set up decompression buffer
    uLongf decompressed_size = 4096;  // Start with an estimated size, adjust as necessary
    vector<Bytef> decompressed_data(decompressed_size);

    // Attempt to decompress; if buffer is too small, resize and retry
    int decompress_result;
    do {
        decompress_result = uncompress(decompressed_data.data(), &decompressed_size,
                                       reinterpret_cast<const Bytef*>(compressed_content.data()), compressed_content.size());
        if (decompress_result == Z_BUF_ERROR) {
            decompressed_size *= 2;  // Double the buffer size and retry
            decompressed_data.resize(decompressed_size);
        } else if (decompress_result != Z_OK) {
            cerr << "Decompression failed with error code: " << decompress_result << endl;
            return;
        }
    } while (decompress_result == Z_BUF_ERROR);

    // Convert the decompressed data to a string
    string tree_content(decompressed_data.begin(), decompressed_data.begin() + decompressed_size);

    // Remove the "tree " header (first 5 characters)
    tree_content = tree_content.substr(5);

    // Parse and display the content line by line
    istringstream stream(tree_content);
    string line;
    while (getline(stream, line)) {
        if (name_only) {
            cout << line.substr(line.rfind(' ') + 1) << endl;  // Print only the filename
        } else {
            cout << line << endl;  // Print the full line
        }
    }
}

string get_current_time() {
    time_t now = time(0);
    char buffer[80];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", localtime(&now));
    return string(buffer);
}

void commit(string message) {
   // string tree_sha = my_write_tree();
    string parent_sha = ""; // Read from HEAD if this isn't the first commit
    string latest_commit="";
    string head_file = ".mygit/HEAD";
    string master_file=".mygit/refs/heads/master";
    ifstream master(master_file);
    if(master.is_open()){
        getline(master,latest_commit);
        master.close();
    }
    if(latest_commit.empty()){
        parent_sha="";
    }
    else{
        parent_sha=latest_commit;
    }
 
    string author = "Chandana Pappula";
    string timestamp = get_current_time();

    // Read the index file to gather all staged files and their hashes
    vector<string> staged_files;
    ifstream index_file(".mygit/index");
    string line;
    while (getline(index_file, line)) {
        // Each line should have a structure like "100644 <hash> <file_path>"
        istringstream iss(line);
        string file_mode, file_hash, file_path;
        iss >> file_mode >> file_hash >> file_path; // Extract components

        // Append only the hash for the commit tree
        staged_files.push_back(line); // Store only the hash for the commit tree
    }
    index_file.close();
    ofstream outFile(".mygit/index", std::ios::trunc);
    if (outFile.is_open()) {
        outFile.close(); // Closing immediately clears the content
    } else {
        std::cerr << "Unable to open file for clearing" << std::endl;
    }
      // Build the commit content with the staged files
    ostringstream commit_content;
   // commit_content << "tree: "; // Start the tree entry
    for (const string& file_sha : staged_files) {
        commit_content << file_sha << endl; // Append each staged file's SHA
    }
   // commit_content << endl;

    if (!parent_sha.empty()) commit_content << "parent sha:"<< parent_sha << endl;
    //else commit_content<<"parent sha:NIL"<<endl;
    commit_content << "author:" << author << " " << timestamp << endl;
    commit_content << "committer:" << author << " " << timestamp << endl;
    commit_content <<"Commit msg:"<<message << endl;

    string commit_data = commit_content.str();
    string commit_sha = compute_sha_hash(commit_data);
    cout << "Commit SHA: " << commit_sha << endl;
    write_as_tree(commit_sha, commit_data);
    // If HEAD is pointing to a branch, update the branch file
        ofstream branch_file(".mygit/refs/heads/master");
    if (branch_file.is_open()) {
        branch_file << commit_sha;
        branch_file.close();
    }
    cout << "HEAD updated to " << commit_sha << " (detached)" << endl;
}
string read_commits(string& commit_hash){
   
    string obj_path = ".mygit/objects/" + commit_hash.substr(0, 2)+"/"+commit_hash.substr(2);

    if (!filesystem::exists(obj_path)) {
        cout << "Object with SHA: " << commit_hash << " does not exist" << endl;
        return "";
    }
    // Open the object file in binary mode and read its compressed content
    ifstream commit_file(obj_path, ios::binary);
    if (!commit_file.is_open()) {
        cerr << "Failed to open object file: " << obj_path << endl;
        return "";
    }
    // Read the compressed data from the file
    stringstream buffer;
    buffer << commit_file.rdbuf();
    string compressed_content = buffer.str();
    commit_file.close();

    // Set up decompression buffer
    uLongf decompressed_size = 4096;  // Start with an estimated size, adjust as necessary
    vector<Bytef> decompressed_data(decompressed_size);

    // Attempt to decompress; if buffer is too small, resize and retry
    int decompress_result;
    do {
        decompress_result = uncompress(decompressed_data.data(), &decompressed_size,
                                       reinterpret_cast<const Bytef*>(compressed_content.data()), compressed_content.size());
        if (decompress_result == Z_BUF_ERROR) {
            decompressed_size *= 2;  // Double the buffer size and retry
            decompressed_data.resize(decompressed_size);
        } else if (decompress_result != Z_OK) {
            cerr << "Decompression failed with error code: " << decompress_result << endl;
            return "";
        }
    } while (decompress_result == Z_BUF_ERROR);

    // Convert the decompressed data to a string
    string tree_content(decompressed_data.begin(), decompressed_data.begin() + decompressed_size);

    //displaying the current commit details    
    cout<<"Commit sha:"<<commit_hash<<endl;
    size_t parent_pos=tree_content.find("parent sha:");
    
    string commit_details;
    if(parent_pos!=string::npos){
        commit_details=tree_content.substr(parent_pos);
    }
    else{
        //no parent commit-->initial commit
        size_t commit_details_pos=tree_content.find("author:");
        cout<<"parent sha:NIL(Initial Commit)"<<endl;
        commit_details=tree_content.substr(commit_details_pos);
    }  
    cout<<commit_details<<endl;
   //size_t parent_pos = tree_content.find("parent sha:");
    if (parent_pos != string::npos) {
        // Extract the parent SHA (40 characters after "parent sha:")
        string parent_sha = tree_content.substr(parent_pos + 11, 40);
       // cout << "Parent SHA: " << parent_sha << endl;
        return parent_sha;
    } else {
       // cout << "No parent SHA found (this might be the first commit)." << endl;
        return "";
    }
    
}
void log_func(){
    ifstream master_file(".mygit/refs/heads/master");
    string commit_hash_latest;
    if(master_file){
        getline(master_file,commit_hash_latest);
    }
    //display the contents of latest commit onwards
    string commit_hash=commit_hash_latest;
    int i=1;
    while(!commit_hash.empty()){
        cout<<"COMMIT LATEST NO."<<i++<<endl;
        commit_hash=read_commits(commit_hash);
    }
}
// Helper function to restore a file's content from its hash value
void restore_file_from_hash(const string& hash, const string& file_path) {
    string obj_path=".mygit/objects/"+hash.substr(0,2)+"/"+hash.substr(2);
    if(!filesystem::exists(obj_path)){
        cout<<"Object with sha:"<<hash<<" does not exist"<<endl;
        return;
    }
    //open the object and decompress it
    ifstream obj_file(obj_path,ios::binary);
    vector<char> comp_data((istreambuf_iterator<char>(obj_file)),istreambuf_iterator<char>());

    uLongf og_size=comp_data.size()*2;
    vector<Bytef> og_data(og_size);

    //adjysut the size until it can hold original data  
    while(uncompress(og_data.data(),&og_size,reinterpret_cast<const Bytef*>(comp_data.data()),comp_data.size())==Z_BUF_ERROR){
        og_size<<1;
        og_data.resize(og_size);
    }
    string og_content(og_data.begin(),og_data.begin()+og_size);
    size_t pos = og_content.find(' ');
    string header = og_content.substr(0, pos);
    string file_content = og_content.substr(pos + 1);

    ofstream restored_file(file_path, ios::binary);
    restored_file<< file_content;
    
    obj_file.close();
    restored_file.close();
}
void checkout_commit(string& commit_sha){
    //openning the commit hash to read the contents of that snapshot
    string obj_path=".mygit/objects/"+commit_sha.substr(0,2)+"/"+commit_sha.substr(2);
    if(!filesystem::exists(obj_path)){
        cout<<"Object with sha:"<<commit_sha<<" does not exist"<<endl;
        return;
    }
    //open the object and decompress it
    ifstream obj_file(obj_path,ios::binary);
    vector<char> comp_data((istreambuf_iterator<char>(obj_file)),istreambuf_iterator<char>());

    uLongf og_size=comp_data.size()*2;
    vector<Bytef> og_data(og_size);

    //adjysut the size until it can hold original data
    while(uncompress(og_data.data(),&og_size,reinterpret_cast<const Bytef*>(comp_data.data()),comp_data.size())==Z_BUF_ERROR){
        og_size<<1;
        og_data.resize(og_size);
    }
    string og_content(og_data.begin(),og_data.begin()+og_size);
    size_t pos = og_content.find(' ');
    string header = og_content.substr(0, pos);
    string file_content = og_content.substr(pos + 1);
    //cout<<file_content<<endl;
    string index_files_info;
    vector<string>files_details;//to store all files info <permission> <hash_val> <file_name>
    cout<<"Files details in commit sha are:"<<endl;
    istringstream iss(file_content);
    string line;

    // Read each line until encountering the "author" section
    while (getline(iss, index_files_info)) {
        if (index_files_info.find("parent sha:") != string::npos || index_files_info.find("author:") != string::npos) {
            break;
        }
        files_details.push_back(index_files_info);
        cout<<index_files_info<<endl;
    }
     // Collect the files/directories to retain based on files_details
    unordered_set<string> files_to_keep;
    for (const auto& file_detail : files_details) {
        istringstream iss(file_detail);
        string permission, hash_value, file_path;
        iss >> permission >> hash_value >> file_path;
        files_to_keep.insert(".mygit/objects/"+hash_value.substr(0,2)+"/"+hash_value.substr(2));
    }  
    files_to_keep.insert(".mygit/objects/"+commit_sha.substr(0,2)+"/"+commit_sha.substr(2));//we should also keep the commit sha blob in-tact
    cout<<"Files to keep are:"<<endl;
    for(auto& x:files_to_keep){
        cout<<x<<endl;
    }
    //Remove any object files not in object_hashes_to_keep from .mygit/objects
    for (const auto& dir_entry : filesystem::recursive_directory_iterator(".mygit/objects")) {
        if (filesystem::is_regular_file(dir_entry)) {
            string object_path = dir_entry.path().string();
            cout<<"OBJ PATH:"<<object_path<<endl; 
            if (filesystem::is_regular_file(dir_entry) && files_to_keep.find(object_path) == files_to_keep.end()) {
                 cout<<"Removing entry:"<<dir_entry<<endl;
                 filesystem::remove(dir_entry);
            }
             // Check if the parent directory (hash.substr(0, 2)) is empty and remove if so
            filesystem::path parent_dir = dir_entry.path().parent_path();
            if (filesystem::is_empty(parent_dir)) {
                cout << "Removing empty directory: " << parent_dir << endl;
                filesystem::remove(parent_dir);
            }
        }
    }
    // // Restore the files listed in files_details
    for (const auto& file_detail : files_details) {
        istringstream iss(file_detail);
        string permission, hash_value, file_path;
        iss >> permission >> hash_value >> file_path;

        // Create directories if they don't exist
        filesystem::path dir_path = filesystem::path(file_path).parent_path();
        cout<<"dir paths to create:"<<dir_path<<endl;
        if (!dir_path.empty() && !filesystem::exists(dir_path)) {
            filesystem::create_directories(dir_path);
            cout<<"created"<<endl;
        }
        // Restore the file content from the object
        restore_file_from_hash(hash_value, file_path);
    }

    // Update master with the checkout commit SHA
    ofstream master_file(".mygit/refs/heads/master");
    if (master_file.is_open()) {
        master_file << commit_sha;
        master_file.close();
    } else {
        cerr << "Failed to update master with the checkout commit SHA." << endl;
    }

    cout << "Checkout to commit " << commit_sha << " completed." << endl;
}
int main(int argc, char *argv[]){

    //INIT OCOMMAND
    if(argc==2 && string(argv[1])=="init"){
        cout<<"In init func"<<endl;
        //creating a .mygit root directory
        string root_dir=".mygit";
        create_dir(root_dir);

        //create subdirectories -->objects,refs/heads
        create_dir(root_dir+"/objects");
        create_dir(root_dir+"/refs");
        create_dir(root_dir+"/refs/heads");

        //creating HEAD file pointing to master branch
        create_file(root_dir+"/HEAD","ref: refs/heads/master\n");
        // Create default master branch
        create_file(".mygit/refs/heads/master", "");
        cout << "Repository initialized successfully." << endl;
        
    }
    //HASH_OBJ
    else if(string(argv[0])=="./mygit" && string(argv[1])=="hash-object"){
        if(argc==3){
            //no flag is given, just compute hash, compress file and output the hash value
            //./mygit hash-object filename

            string file_name=argv[2];
            //open file
            ifstream my_file(file_name, ios::binary);
            if(!my_file){
                cout<<"Unable to open file "<<file_name<<" to compute sha1 hash"<<endl;
                return 0;
            }

            //read file into a string
            ostringstream out_ss;
            out_ss << my_file.rdbuf();
            string file_content= out_ss.str();
            string sha1_val=compute_sha_hash(file_content);
            if(sha1_val.empty())
                cout<<"check file "<<file_name<<endl;
            else
                cout<<"SHA-1 for file "<<file_name<<": "<<sha1_val<<endl;
        }
        else if(argc==4){
            string flag=argv[2];
            string file_name=argv[3];
            ifstream my_file(file_name, ios::binary);
            if(!my_file){
                cout<<"Unable to open file "<<file_name<<" to compute sha1 hash"<<endl;
                return 0;
            }

            //read file into a string
            ostringstream out_ss;
            out_ss << my_file.rdbuf();
            string file_content= out_ss.str();
            //we need calculate the sha-1 hash for the file and compress it 
            //if flag=="-w", write the file as blob oject to the repo
            //display the sha-1 hash value on screen
            string sha1_val=compute_sha_hash(file_content);
            cout<<"SHA-1 for file "<<file_name<<": "<<sha1_val<<endl;
            //write to blob as we have -w flag
            if(flag=="-w"){
                write_as_blob(sha1_val,file_name);
                
            }
            
        }
    }
    //CAT_FILE
    else if(string(argv[0])=="./mygit" && string(argv[1])=="cat-file"){
        if(argc!=4){
            cout<<"Invalid number of arguments"<<endl;
            return 0;
        }
        else{
            string flag=argv[2];
            string file_sha=argv[3];
            cat_file_func(flag,file_sha);
        }
    }
    //WRITE_TREE
    else if(string(argv[1])=="write-tree"){
        if(argc!=2){
            cout<<"Invalid number of arguments"<<endl;
            return 0;
        }
        //filesystem::path curr_dir=filesystem::current_path();
        string tree_sha1_val= my_write_tree(".");
        cout<<"Tree sha1: "<<tree_sha1_val;
    }
    //ADD COMMAND
    else if(string(argv[1]) == "add"){
        if (argc == 3 && string(argv[2]) == ".") {
            add_all_files();
        } 
        else if (argc > 2 && string(argv[1]) == "add") {
            vector<string> objs;
            for (int i = 2; i < argc; ++i) {
                objs.push_back(argv[i]);
            }
            add_files(objs);
        }
    }
    //LS_TREE
    else if(string(argv[1])=="ls-tree"){
        if(argc==3){
            string tree_sha=string(argv[2]);
            ls_tree(tree_sha,false);
        }
        else if(argc==4 && string(argv[2])=="--name-only"){
            string tree_sha=string(argv[3]);
            ls_tree(tree_sha,true);
        }
    }
    //COMMIT COMMAND
    else if(string(argv[1])=="commit"){
        string commit_msg;
        if(argc==2){
            commit_msg="Commit done by Chandu\n";
        }
        else if(string(argv[2])=="-m"){
            for(int i=3;i<argc;i++)
                commit_msg+=string(argv[i])+" ";
        }
        commit(commit_msg);
    }
    //LOG COMMAND
    else if(string(argv[1])=="log" && argc==2){
        log_func();
    }
    //CHECK_OUT
    else if(argc==3 && string(argv[1])=="checkout"){
        string commit_sha=argv[2];
        checkout_commit(commit_sha);
    }
    return 0;
}