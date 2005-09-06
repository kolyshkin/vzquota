#ifndef __KCOMPAT_H__
#define __KCOMPAT_H__

/*
 * This header file defines kernel structures for old quota interfaces
 */
#ifndef Q_SETGRACE

#define Q_SETGRACE 0x0B00       /* set inode and block grace */
#define Q_SETQLIM  0x0700       /* set limits */


/* This is in-memory copy of quota block. See meaning of entries above */
struct mem_dqblk {
	unsigned int dqb_ihardlimit;
	unsigned int dqb_isoftlimit;
	unsigned int dqb_curinodes;
	unsigned int dqb_bhardlimit;
	unsigned int dqb_bsoftlimit;
	qsize_t dqb_curspace;
	__kernel_time_t dqb_btime;
	__kernel_time_t dqb_itime;
};

/* Inmemory copy of version specific information */
struct mem_dqinfo {
	unsigned int dqi_bgrace;
	unsigned int dqi_igrace;
	unsigned int dqi_flags;
	unsigned int dqi_blocks;
	unsigned int dqi_free_blk;
	unsigned int dqi_free_entry;
};

#endif

#endif /* __KCOMPAT_H__ */
