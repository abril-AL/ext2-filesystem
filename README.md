## UID: 605805712


# Hey! I'm Filing Here

This program creates a 1 MiB ext2 file system with 2 directories, 1 regular file, and 1 symbolic link.

## Building

To create the image of the file system
```
make && ./ext2-create
```
Before you mount, the image should be a valid file system
Check this using:
```
fsck.ext2 cs111-base.img
```


## Running

Now we can mount this image
First create the directory to mount the file system to 
```
mkdir mnt
```
Then mount:
```
sudo mount -o loop cs111-base.img mnt
```
An example output of `ls -ain mnt` is:
```
2 drwxr -xr -x 3 0 0 1024 .
 (this will differ)       ..
13 lrw -r--r-- 1 1000 1000 11 hello -> hello - world
12 -rw -r--r-- 1 1000 1000 12 hello - world
11 drwxr -xr -x 2 0 0 1024 lost+ found
```

## Cleaning up

To unmount and delete the directory use:
```
sudo umount mnt && rmdir mnt
```
And to clean up binary files use:
```
make clean
```
