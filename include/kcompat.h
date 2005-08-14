#ifndef __KCOMPAT_H__
#define __KCOMPAT_H__

/*
 * This header file defines kernel structures which used to be
 * defined in previous glibc-kernel-headers, but for some unknown
 * reason were removed from there in recent glibc.
 * vzquota requires these defines...
 */
#ifndef IIF_BGRACE

#define IIF_BGRACE	1
#define IIF_IGRACE	2
#define QIF_BLIMITS	1
#define QIF_ILIMITS	4
#define QIF_LIMITS	(QIF_BLIMITS | QIF_ILIMITS)

struct if_dqblk {
	__u64 dqb_bhardlimit;
	__u64 dqb_bsoftlimit;
	__u64 dqb_curspace;
	__u64 dqb_ihardlimit;
	__u64 dqb_isoftlimit;
	__u64 dqb_curinodes;
	__u64 dqb_btime;
	__u64 dqb_itime;
	__u32 dqb_valid;
};

struct if_dqinfo {
	__u64 dqi_bgrace;
	__u64 dqi_igrace;
	__u32 dqi_flags;
	__u32 dqi_valid;
};

#endif

#endif /* __KCOMPAT_H__ */
