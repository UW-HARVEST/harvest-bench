# Build Dependencies

To build SQLite you need the following dependencies:
* zlib (if building with `-DSQLITE_WITH_ZLIB`)
* libedit (if building with `-DSQLITE_WITH_EDITLIN`)
* TCL 8.6 (if building with `-DSQLITE_TEST`)

To install these with apt run:
```
sudo apt install zlib1g-dev libedit-dev tcl-dev
```
