//Command to execute
g++ -o mygit main.cpp -lssl -lcrypto -lz

COMMANDS:

## 1.init

-->Initializes .mygit repository by creating
.mygit/objects
.mygit/refs/master -->Contains the latest commit sha of current branch(master)
.mygit.HEAD -->points to the initial working branch master
-->Sets up the basic repository structure

## 2.hash-object

./mygit hash-object [-w] file_name

-->without flag, just compute the hash-value for the file content and print it
-->with flag , create a blob object with 1st two digits of its sha-value in the .mygit/objects folder

## 3.cat-file

./mygit cat-file <flag> <file_name>

flags: -p: displays the content of file
-s: displays the size of the file
-t: displays the type of the file(tree/blob)

## 4.write-tree

./mygit write-tree

-->It takes the current working directory and computes the hash on the directory structure and the hash is stored in ./mygit/objects
-->here I'm excluding ./mygit(repo),mygit(exe file),main.cpp,.vscode files from the write-tree command

## 5.List Tree

./mygit ls-tree [--name-only] <tree_sha>

-->Given a tree_sha value it prints the structure of tree contents and their details
-->with --name-only, it prints only the directory & file names in the tree structure

## 6.Add Files

For the 1st time staging , INDEX file gets created

./mygit add . -->add all files in the cuurent directory to staging area
./mygit add file1 file2... --> adds the mentioned files to staging area

## 7.Commit Changes

./mygit commit [-m] [commit_msg]

When we use commit command, it creates a new commit object, all the file details in staging area are pushed as a batch to objects folder by computing the commit hash for the commit object.
t
When the commit occurs, index file is erased as all of them are committed now, and the master file in .mygit/refs/master points to the new commit object

## 8.Log Command

./mygit log

It displays all the commits information from latest to oldest in the below format

Commit No.
COmmit sha:
Parent sha:
author:
Committer:
Commit msg:

## 9.Checkout Command

./mygit checkout <commit_hash>

This checks whether the given commit object exists or not and roll backs the system to that commit object
It opens the commit object and reads the files from the commit object and changes the system to that state, by erasing the blobs not present in the commit object from .mygit/object folder
