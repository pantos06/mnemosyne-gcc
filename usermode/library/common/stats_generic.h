/**
 * \file stats.h 
 *
 * \brief Statistics collector interface and MACROs.
 *
 * FIXME: This statistics collector was meant to be as generic as possible
 * but it seems that it didn't live up to its promise. This is because the 
 * logic is based on the logic of the statistics collector used in the 
 * transaction statistics collector used by MTM. For the future, we would 
 * like to implement a more extensible statistics collector similar to the
 * one used by Solaris kstat provider.
 *
 */

#ifndef _M_STATS_H
#define _M_STATS_H

#include "result.h"


/** The statistics collected by the collector. */
#define FOREACH_STAT_XACT(ACTION)                                           \
  ACTION(aborts)                                                            \


#ifdef _M_STATS_BUILD
/** 
 * Declare under FOREACH_STAT and FOREACH_STATPROBE the statistics you want 
 * to generate probes for and under FOREACH_VOIDSTATPROBE the statistics you
 * do NOT want to generate probes for (i.e. generate void probes).
 *
 * Important: FOREACH_STAT? and FOREACH_VOIDSTATPROBE must be mutually 
 * exclusive. 
 */
# define FOREACH_STAT(ACTION)                                                \
    FOREACH_STAT_XACT (ACTION)

# define FOREACH_STATPROBE(ACTION)                                           \
    ACTION(XACT)

# define FOREACH_VOIDSTATPROBE(ACTION)                                           


#else /* !_M_STATS_BUILD */

# define FOREACH_STAT(ACTION)          /* do nothing */
# define FOREACH_STATPROBE(ACTION)     /* do nothing */
# define FOREACH_VOIDSTATPROBE(ACTION) /* do nothing */

#endif /* !_M_STATS_BUILD */

typedef enum {
#define STATENTRY(name) m_stats_##name##_stat,
	FOREACH_STAT (STATENTRY)
#undef STATENTRY	
	m_stats_numofstats
} m_stats_statentry_t; 

typedef unsigned int m_stats_statcounter_t;
typedef struct m_stats_threadstat_s m_stats_threadstat_t;
typedef struct m_statsmgr_s m_statsmgr_t;
typedef struct m_stats_statset_s m_stats_statset_t;
typedef struct m_stats_stat_s m_stats_stat_t;


/** Statistic */
struct m_stats_stat_s
{
	m_stats_statcounter_t max;   /**< Maximum number of the statistic across all transactions of an atomic block (static transaction). */
	m_stats_statcounter_t min;   /**< Minimum number of the statistic across all transactions of an atomic block (static transaction). */
	m_stats_statcounter_t total; /**< Total number of the statistic across all transactions of an atomic block (static transaction). */
}; 


/**Statistics set */
struct m_stats_statset_s {
	const char             *name;       /**< Name tag. */
	m_stats_statcounter_t  count;       /**< Number of instances. */
	m_stats_stat_t         stats[m_stats_numofstats]; /**< Statistics collection. */
}; 


/** \brief Writes the value of a statistic. */
static inline
void
m_stats_statset_set_val(m_stats_statset_t *statset, 
                        m_stats_statentry_t entry,
                        m_stats_statcounter_t val)
{
	statset->stats[entry].total = val;	
}


/** \brief Reads the value of a statistic. */
static inline
m_stats_statcounter_t
m_stats_statset_get_val(m_stats_statset_t *statset, 
                        m_stats_statentry_t entry)
{
	return statset->stats[entry].total;
}


#define GENERATE_STATPROBES(stat_provider)                                     \
static inline                                                                  \
void                                                                           \
m_stats_statset_increment_##stat_provider(m_statsmgr_t *statsmgr,              \
                                          m_stats_statset_t *statset,          \
                                          m_stats_statentry_t entry,           \
                                          m_stats_statcounter_t val)           \
{                                                                              \
    if (statsmgr) {                                                            \
        m_stats_statset_set_val(statset,                                       \
                                entry,                                         \
                                m_stats_statset_get_val(statset, entry) + val);\
    }							                                               \
}										                                       \
                                                                               \
static inline                                                                  \
void                                                                           \
m_stats_statset_decrement_##stat_provider(m_statsmgr_t *statsmgr,              \
                                          m_stats_statset_t *statset,          \
                                          m_stats_statentry_t entry,           \
                                          m_stats_statcounter_t val)           \
{                                                                              \
	if (statsmgr) {                                                            \
        m_stats_statset_set_val(statset,                                       \
                                entry,                                         \
                                m_stats_statset_get_val(statset, entry) - val);\
    }							                                               \
}										                                       \


#define GENERATE_VOIDSTATPROBES(stat_provider)                               \
static inline                                                                \
void                                                                         \
m_stats_statset_increment_##stat_provider(m_statsmgr_t *statsmgr,            \
                                          m_stats_statset_t *statset,        \
                                          m_stats_statentry_t entry,         \
                                          m_stats_statcounter_t val)         \
{                                                                            \
	return ;                                                                 \
}										                                     \
                                                                             \
static inline                                                                \
void                                                                         \
m_stats_statset_decrement_##stat_provider(m_statsmgr_t *statsmgr,            \
                                          m_stats_statset_t *statset,        \
                                          m_stats_statentry_t entry,         \
                                          m_stats_statcounter_t val)         \
{                                                                            \
	return ;                                                                 \
}										                                     \


FOREACH_STATPROBE (GENERATE_STATPROBES)
FOREACH_VOIDSTATPROBE (GENERATE_VOIDSTATPROBES)


#define m_stats_statset_increment(statsmgr, statset, stat_provider, stat, val) \
  m_stats_statset_increment_##stat_provider(statsmgr,                          \
                                            statset,                           \
											m_stats_##stat##_stat,             \
                                            val);

#define m_stats_statset_decrement(statsmgr, statset, stat_provider, stat, val) \
  m_stats_statset_decrement_##stat_provider(statsmgr,                          \
                                            statset,                           \
                                            m_stats_##stat##_stat,             \
                                            val);


m_result_t m_statsmgr_create(m_statsmgr_t **statsmgrp, char *output_file);
m_result_t m_statsmgr_destroy(m_statsmgr_t **statsmgrp);
m_result_t m_stats_threadstat_create(m_statsmgr_t *statsmgr, unsigned int tid, m_stats_threadstat_t **threadstatp);
m_result_t m_stats_statset_create(m_stats_statset_t **statsetp);
m_result_t m_stats_statset_destroy(m_stats_statset_t **statsetp);
m_result_t m_stats_statset_init(m_stats_statset_t *statset, const char *name);
void m_stats_print(m_statsmgr_t *statsmgr);

#endif /* _M_STATS_H */
