#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int16_t i16;
typedef int32_t i32;

#define BLOCK_SIZE 1024
#define BLOCK_OFFSET(i) (i * BLOCK_SIZE)
#define NUM_BLOCKS 1024
#define NUM_INODES 128

#define LOST_AND_FOUND_INO 11
#define HELLO_WORLD_INO    12
#define HELLO_INO          13
#define LAST_INO           HELLO_INO

#define SUPERBLOCK_BLOCKNO             1
#define BLOCK_GROUP_DESCRIPTOR_BLOCKNO 2
#define BLOCK_BITMAP_BLOCKNO           3
#define INODE_BITMAP_BLOCKNO           4
#define INODE_TABLE_BLOCKNO            5
#define ROOT_DIR_BLOCKNO               21
#define LOST_AND_FOUND_DIR_BLOCKNO     22
#define HELLO_WORLD_FILE_BLOCKNO       23
#define LAST_BLOCK                     HELLO_WORLD_FILE_BLOCKNO

#define NUM_FREE_BLOCKS (NUM_BLOCKS - LAST_BLOCK - 1)
#define NUM_FREE_INODES (NUM_INODES - LAST_INO)

/* http://www.nongnu.org/ext2-doc/ext2.html */
/* http://www.science.smith.edu/~nhowe/262/oldlabs/ext2.html */

#define EXT2_MAGIC_NUMBER 0xEF53; 

#define	EXT2_BAD_INO             1
#define EXT2_ROOT_INO            2
#define EXT2_GOOD_OLD_FIRST_INO 11

#define EXT2_GOOD_OLD_REV 0

#define EXT2_S_IFSOCK 0xC000
#define EXT2_S_IFLNK  0xA000
#define EXT2_S_IFREG  0x8000
#define EXT2_S_IFBLK  0x6000
#define EXT2_S_IFDIR  0x4000
#define EXT2_S_IFCHR  0x2000
#define EXT2_S_IFIFO  0x1000
#define EXT2_S_ISUID  0x0800
#define EXT2_S_ISGID  0x0400
#define EXT2_S_ISVTX  0x0200
#define EXT2_S_IRUSR  0x0100
#define EXT2_S_IWUSR  0x0080
#define EXT2_S_IXUSR  0x0040
#define EXT2_S_IRGRP  0x0020
#define EXT2_S_IWGRP  0x0010
#define EXT2_S_IXGRP  0x0008
#define EXT2_S_IROTH  0x0004
#define EXT2_S_IWOTH  0x0002
#define EXT2_S_IXOTH  0x0001

#define	EXT2_NDIR_BLOCKS 12
#define	EXT2_IND_BLOCK   EXT2_NDIR_BLOCKS
#define	EXT2_DIND_BLOCK  (EXT2_IND_BLOCK + 1)
#define	EXT2_TIND_BLOCK  (EXT2_DIND_BLOCK + 1)
#define	EXT2_N_BLOCKS    (EXT2_TIND_BLOCK + 1)

#define EXT2_NAME_LEN 255

struct ext2_superblock {
	u32 s_inodes_count;
	u32 s_blocks_count;
	u32 s_r_blocks_count;
	u32 s_free_blocks_count;
	u32 s_free_inodes_count;
	u32 s_first_data_block;
	u32 s_log_block_size;
	i32 s_log_frag_size;
	u32 s_blocks_per_group;
	u32 s_frags_per_group;
	u32 s_inodes_per_group;
	u32 s_mtime;
	u32 s_wtime;
	u16 s_mnt_count;
	i16 s_max_mnt_count;
	u16 s_magic;
	u16 s_state;
	u16 s_errors;
	u16 s_minor_rev_level;
	u32 s_lastcheck;
	u32 s_checkinterval;
	u32 s_creator_os;
	u32 s_rev_level;
	u16 s_def_resuid;
	u16 s_def_resgid;
	u32 s_pad[5];
	u8 s_uuid[16];
	u8 s_volume_name[16];
	u32 s_reserved[229];
};

struct ext2_block_group_descriptor
{
	u32 bg_block_bitmap;
	u32 bg_inode_bitmap;
	u32 bg_inode_table;
	u16 bg_free_blocks_count;
	u16 bg_free_inodes_count;
	u16 bg_used_dirs_count;
	u16 bg_pad;
	u32 bg_reserved[3];
};

struct ext2_inode {
	u16 i_mode;
	u16 i_uid;
	u32 i_size;
	u32 i_atime;
	u32 i_ctime;
	u32 i_mtime;
	u32 i_dtime;
	u16 i_gid;
	u16 i_links_count;
	u32 i_blocks;
	u32 i_flags;
	u32 i_reserved1;
	u32 i_block[EXT2_N_BLOCKS];
	u32 i_version;
	u32 i_file_acl;
	u32 i_dir_acl;
	u32 i_faddr;
	u8  i_frag;
	u8  i_fsize;
	u16 i_pad1;
	u32 i_reserved2[2];
};

struct ext2_dir_entry {
	u32 inode;
	u16 rec_len;
	u16 name_len;
	u8  name[EXT2_NAME_LEN];
};

#define errno_exit(str)                                                        \
	do { int err = errno; perror(str); exit(err); } while (0)

#define dir_entry_set(entry, inode_num, str)                                   \
	do {                                                                   \
		char *s = str;                                                 \
		size_t len = strlen(s);                                        \
		entry.inode = inode_num;                                       \
		entry.name_len = len;                                          \
		memcpy(&entry.name, s, len);                                   \
		if ((len % 4) != 0) {                                          \
			entry.rec_len = 12 + len / 4 * 4;                      \
		}                                                              \
		else {                                                         \
			entry.rec_len = 8 + len;                               \
		}                                                              \
	} while (0)

#define dir_entry_write(entry, fd)                                             \
	do {                                                                   \
		size_t size = entry.rec_len;                                   \
		if (write(fd, &entry, size) != size) {                         \
			errno_exit("write");                                   \
		}                                                              \
	} while (0)

u32 get_current_time() {
	time_t t = time(NULL);
	if (t == ((time_t) -1)) {
		errno_exit("time");
	}
	return t;
}

void write_superblock(int fd) {
	off_t off = lseek(fd, BLOCK_OFFSET(1), SEEK_SET);
	if (off == -1) {
		errno_exit("lseek");
	}

	u32 current_time = get_current_time();

	struct ext2_superblock superblock = {0};

	/* These are intentionally incorrectly set as 0, you should set them
	   correctly and delete this comment */
	superblock.s_inodes_count      = NUM_INODES;
	superblock.s_blocks_count      = NUM_BLOCKS;
	superblock.s_r_blocks_count    = 0;
	superblock.s_free_blocks_count = NUM_FREE_BLOCKS;
	superblock.s_free_inodes_count = NUM_FREE_INODES;
	superblock.s_first_data_block  = SUPERBLOCK_BLOCKNO; /* First Data Block */
	superblock.s_log_block_size    = 0; /* 1024 */
	superblock.s_log_frag_size     = 0; /* 1024 */
	superblock.s_blocks_per_group  = BLOCK_SIZE * 8;//num of blocks we can rep in a group,
	superblock.s_frags_per_group   = BLOCK_SIZE * 8;// so size of block * 8
	superblock.s_inodes_per_group  = NUM_INODES;
	superblock.s_mtime             = 0; /* Mount time */
	superblock.s_wtime             = current_time; /* Write time */
	superblock.s_mnt_count         = 0; /* Number of times mounted so far */
	superblock.s_max_mnt_count     = 0; /* Make this unlimited */
	superblock.s_magic             = EXT2_MAGIC_NUMBER; /* ext2 Signature */
	superblock.s_state             = 0; /* File system is clean */
	superblock.s_errors            = 0; /* Ignore the error (continue on) */
	superblock.s_minor_rev_level   = 0; /* Leave this as 0 */
	superblock.s_lastcheck         = current_time; /* Last check time */
	superblock.s_checkinterval     = 0; /* Force checks by making them every 1 second */
	superblock.s_creator_os        = 0; /* Linux */
	superblock.s_rev_level         = 0; /* Leave this as 0 */
	superblock.s_def_resuid        = 0; /* root */
	superblock.s_def_resgid        = 0; /* root */

	/* You can leave everything below this line the same, delete this
	   comment when you're done the lab */
	superblock.s_uuid[0] = 0x5A;
	superblock.s_uuid[1] = 0x1E;
	superblock.s_uuid[2] = 0xAB;
	superblock.s_uuid[3] = 0x1E;
	superblock.s_uuid[4] = 0x13;
	superblock.s_uuid[5] = 0x37;
	superblock.s_uuid[6] = 0x13;
	superblock.s_uuid[7] = 0x37;
	superblock.s_uuid[8] = 0x13;
	superblock.s_uuid[9] = 0x37;
	superblock.s_uuid[10] = 0xC0;
	superblock.s_uuid[11] = 0xFF;
	superblock.s_uuid[12] = 0xEE;
	superblock.s_uuid[13] = 0xC0;
	superblock.s_uuid[14] = 0xFF;
	superblock.s_uuid[15] = 0xEE;

	memcpy(&superblock.s_volume_name, "cs111-base", 10);

	ssize_t size = sizeof(superblock);
	if (write(fd, &superblock, size) != size) {
		errno_exit("write");
	}
}

void write_block_group_descriptor_table(int fd) {
	off_t off = lseek(fd, BLOCK_OFFSET(2), SEEK_SET);
	if (off == -1) {
		errno_exit("lseek");
	}
	
	struct ext2_block_group_descriptor block_group_descriptor = {0};

	block_group_descriptor.bg_block_bitmap = BLOCK_BITMAP_BLOCKNO ;
	block_group_descriptor.bg_inode_bitmap = INODE_BITMAP_BLOCKNO ;
	block_group_descriptor.bg_inode_table =  INODE_TABLE_BLOCKNO  ;
	block_group_descriptor.bg_free_blocks_count = 1000;
	block_group_descriptor.bg_free_inodes_count = 115;
	block_group_descriptor.bg_used_dirs_count = 2;//????

	ssize_t size = sizeof(block_group_descriptor);
	if (write(fd, &block_group_descriptor, size) != size) {
		errno_exit("write");
	}
}

void write_block_bitmap(int fd) {

	off_t off = lseek(fd, BLOCK_OFFSET(BLOCK_BITMAP_BLOCKNO), SEEK_SET);
	if (off == -1) {
		errno_exit("lseek");
	}

	//every int = 4 bytes = 32 bits , 1024/32 = 32
	//whole block (1024 * 8) reps 8192 bits, 8192/(4*8) = 256 ints
	const unsigned int bm_size = 32;// 0-1023
	unsigned int bm_array[256];//1024-8192
	// block 1-24 are taken so:
	bm_array[0] = 0x007fffff;
	for (unsigned int i = 1; i < bm_size; i++){
		bm_array[i] = 0x00000000;//fill bitmap in w zeros
	}
	for (unsigned int i = bm_size; i < 256; i++){
		bm_array[i] = 0xffffffff;//fill rest in w ones
	}
	bm_array[bm_size-1] = 0x80000000;//bc 0th doesnt count

	ssize_t size = sizeof(bm_array);
	if (write(fd, &bm_array, size) != size) {
		errno_exit("write");
	}
}

void write_inode_bitmap(int fd) {
	off_t off = lseek(fd, BLOCK_OFFSET(INODE_BITMAP_BLOCKNO), SEEK_SET);
	if (off == -1) {
		errno_exit("lseek");
	}

	//every int = 4 bytes = 32 bits , 128/32 = 4
	const int bm_size = 4;// 0-127
	unsigned int bm_array[256];
	for (int i = 0; i < bm_size; i++){
		bm_array[i] = 0;//fill bitmap in w zeros
	}
	// inodes 0-14 are taken so: 0001 1111 1111 1111
	bm_array[0] = 0x1fff;
	for (int i = bm_size; i < 256; i++){
		bm_array[i] = 0xffffffff;//fill in rest of the block 
	}
	
	ssize_t size = BLOCK_SIZE;
	if (write(fd, &bm_array, size) != size) {
		errno_exit("write");
	}
}

void write_inode(int fd, u32 index, struct ext2_inode *inode) {
	off_t off = BLOCK_OFFSET(INODE_TABLE_BLOCKNO)
	            + (index - 1) * sizeof(struct ext2_inode);
	off = lseek(fd, off, SEEK_SET);
	if (off == -1) {
		errno_exit("lseek");
	}

	ssize_t size = sizeof(struct ext2_inode);
	if (write(fd, inode, size) != size) {
		errno_exit("write");
	}
}

void write_inode_table(int fd) {
	u32 current_time = get_current_time();

	struct ext2_inode lost_and_found_inode = {0};
	lost_and_found_inode.i_mode = EXT2_S_IFDIR
	                              | EXT2_S_IRUSR
	                              | EXT2_S_IWUSR
	                              | EXT2_S_IXUSR
	                              | EXT2_S_IRGRP
	                              | EXT2_S_IXGRP
	                              | EXT2_S_IROTH
	                              | EXT2_S_IXOTH;
	lost_and_found_inode.i_uid = 0;
	lost_and_found_inode.i_size = 1024;
	lost_and_found_inode.i_atime = current_time;
	lost_and_found_inode.i_ctime = current_time;
	lost_and_found_inode.i_mtime = current_time;
	lost_and_found_inode.i_dtime = 0;
	lost_and_found_inode.i_gid = 0;
	lost_and_found_inode.i_links_count = 2;
	lost_and_found_inode.i_blocks = 2; /* These are oddly 512 blocks */
	lost_and_found_inode.i_block[0] = LOST_AND_FOUND_DIR_BLOCKNO;
	write_inode(fd, LOST_AND_FOUND_INO, &lost_and_found_inode);

	/*You’ll be creating 4 inodes: the root directory, the lost+found directory, a regular
	file named hello-world, and a symbolic link named hello which points to hello-world*/

	struct ext2_inode root_inode = {0};
	root_inode.i_mode = EXT2_S_IFDIR
			        	| EXT2_S_IRUSR
			        	| EXT2_S_IWUSR
			        	| EXT2_S_IXUSR
			        	| EXT2_S_IRGRP
			        	| EXT2_S_IXGRP
			        	| EXT2_S_IROTH
			        	| EXT2_S_IXOTH;
	root_inode.i_uid = 0;
	root_inode.i_size = 1024;
	root_inode.i_atime = current_time;
	root_inode.i_ctime = current_time;
	root_inode.i_mtime = current_time;
	root_inode.i_dtime = 0;
	root_inode.i_gid = 0;
	root_inode.i_links_count = 3;//'.' and '..' and lost+found
	root_inode.i_blocks = 2; /* These are oddly 512 blocks, 1024 total*/
	root_inode.i_block[0] = ROOT_DIR_BLOCKNO;
	write_inode(fd, EXT2_ROOT_INO, &root_inode);

	struct ext2_inode reg_file_inode = {0};
	reg_file_inode.i_mode = EXT2_S_IFREG
						    | EXT2_S_IRUSR
						    | EXT2_S_IWUSR
						    | EXT2_S_IRGRP
						    | EXT2_S_IROTH;
	reg_file_inode.i_uid = 1000;
	reg_file_inode.i_size = 12;
	reg_file_inode.i_atime = current_time;
	reg_file_inode.i_ctime = current_time;
	reg_file_inode.i_mtime = current_time;
	reg_file_inode.i_dtime = 0;
	reg_file_inode.i_gid = 1000;
	reg_file_inode.i_links_count = 1;//not a dir, and symlink doesnt add to it, so only itself (inode)
	reg_file_inode.i_blocks = 2; // These are oddly 512 blocks
	reg_file_inode.i_block[0] = HELLO_WORLD_FILE_BLOCKNO ;
	write_inode(fd, HELLO_WORLD_INO, &reg_file_inode);


	struct ext2_inode symlink_inode = {0};
	symlink_inode.i_mode = EXT2_S_IFLNK
						   | EXT2_S_IRUSR
						   | EXT2_S_IWUSR
						   | EXT2_S_IRGRP
						   | EXT2_S_IROTH;
	symlink_inode.i_uid = 1000;
	symlink_inode.i_size = 11;
	symlink_inode.i_atime = current_time;
	symlink_inode.i_ctime = current_time;
	symlink_inode.i_mtime = current_time;
	symlink_inode.i_dtime = 0;
	symlink_inode.i_gid = 1000;
	symlink_inode.i_links_count = 1;//only itself 
	symlink_inode.i_blocks = 0; // These are oddly 512 blocks 
	char arr[12] = "hello-world";
	memcpy(symlink_inode.i_block,arr,11);
	write_inode(fd, HELLO_INO, &symlink_inode);
}

void write_root_dir_block(int fd) {
	
	off_t off = BLOCK_OFFSET(ROOT_DIR_BLOCKNO);
	off = lseek(fd, off, SEEK_SET);
	if (off == -1) {
		errno_exit("lseek");
	}

	ssize_t bytes_remaining = BLOCK_SIZE;

	struct ext2_dir_entry current_entry = {0};//init. all fields to zero
	dir_entry_set(current_entry, EXT2_ROOT_INO, ".");
	dir_entry_write(current_entry, fd);

	bytes_remaining -= current_entry.rec_len;

	struct ext2_dir_entry parent_entry = {0};
	dir_entry_set(parent_entry, EXT2_ROOT_INO, "..");//parent of root is root
	dir_entry_write(parent_entry, fd);

	bytes_remaining -= parent_entry.rec_len;

	struct ext2_dir_entry child_entry1 = {0};
	dir_entry_set(child_entry1, LOST_AND_FOUND_INO, "lost+found");
	dir_entry_write(child_entry1, fd);

	bytes_remaining -= child_entry1.rec_len;

	struct ext2_dir_entry child_entry2 = {0};
	dir_entry_set(child_entry2, HELLO_WORLD_INO, "hello-world");
	dir_entry_write(child_entry2, fd);

	bytes_remaining -= child_entry2.rec_len;

	struct ext2_dir_entry child_entry3 = {0};
	dir_entry_set(child_entry3, HELLO_INO, "hello");
	dir_entry_write(child_entry3, fd);

	bytes_remaining -= child_entry3.rec_len;

	struct ext2_dir_entry fill_entry = {0};
	fill_entry.rec_len = bytes_remaining;
	dir_entry_write(fill_entry, fd);
	

}

void write_lost_and_found_dir_block(int fd) {
	//change offset to write at the beginning of the block
	off_t off = BLOCK_OFFSET(LOST_AND_FOUND_DIR_BLOCKNO);
	//lseek to change the position of the write to the file
	off = lseek(fd, off, SEEK_SET);
	if (off == -1) {
		errno_exit("lseek");
	}

	//need to track how many bytes are left for the last entry , need to fill whole block w fds (?)
	ssize_t bytes_remaining = BLOCK_SIZE;

	//create dir entry for the current directory
	struct ext2_dir_entry current_entry = {0};//init. all fields to zero
	//macro that takes a dir entry, an inode number, and assigns it a name
	dir_entry_set(current_entry, LOST_AND_FOUND_INO, ".");
	dir_entry_write(current_entry, fd);

	bytes_remaining -= current_entry.rec_len;

	struct ext2_dir_entry parent_entry = {0};
	dir_entry_set(parent_entry, EXT2_ROOT_INO, "..");
	dir_entry_write(parent_entry, fd);

	bytes_remaining -= parent_entry.rec_len;

	struct ext2_dir_entry fill_entry = {0};
	fill_entry.rec_len = bytes_remaining;
	dir_entry_write(fill_entry, fd);
}

void write_hello_world_file_block(int fd) {
	off_t off = BLOCK_OFFSET(HELLO_WORLD_FILE_BLOCKNO);
	off = lseek(fd, off, SEEK_SET);
	if (off == -1) {
		errno_exit("lseek");
	}
	char arr[12] = "Hello world\n";
	ssize_t size = sizeof(arr);
	if (write(fd, arr, size) != size) {
		errno_exit("write");
	}
}

int main(int argc, char *argv[]) {
	int fd = open("cs111-base.img", O_CREAT | O_WRONLY, 0666);
	if (fd == -1) {
		errno_exit("open");
	}

	if (ftruncate(fd, 0)) {
		errno_exit("ftruncate");
	}
	if (ftruncate(fd, NUM_BLOCKS * BLOCK_SIZE)) {
		errno_exit("ftruncate");
	}

	write_superblock(fd);
	write_block_group_descriptor_table(fd);
	write_block_bitmap(fd);
	write_inode_bitmap(fd);
	write_inode_table(fd);
	write_root_dir_block(fd);
	write_lost_and_found_dir_block(fd);
	write_hello_world_file_block(fd);

	if (close(fd)) {
		errno_exit("close");
	}
	return 0;
}
