LINFO1252 - P2 : Read a tar archive
---
We had to implement the functions 
- `check_archive` : checks if all headers present in the tar are valid
- `is_dir`, `is_file`, `is_sym` : each one check if the file at the given path is respectively a directory, a file or a symlink
- `list` : lists all file and subdirectories present at first level in given directory
- `read_file` : reads the content of a file at a given path

A couple of handy methods have been defined 
The hardest part was understanding the structure of a tar archive and how to read the blocks (and thus the entries).
Once done, it's quite quick to write the functions.

The chosen approach uses `mmap` to map the content of the archive to an array of `tar_block_header` each one having a size of `512` bytes.
Using `statbuf.size/sizeof(tar_block_header)`, we had the amount of blocks in the array. Used this array to while loop over each block, checking if a block is valid header a processing the information in consequence.

