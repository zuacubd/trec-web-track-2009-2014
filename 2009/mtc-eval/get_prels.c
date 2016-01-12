/* Copyright (c) 2003, 1991, 1990, 1984 Chris Buckley.  */


#include "common.h"
#include "sysfunc.h"
#include "smart_error.h"
#include "trec_eval.h"
#include <ctype.h>


/* Read all relevance information from text_qrels_file.
Relevance for each docno to qid is determined from text_qrels_file, which
consists of text tuples of the form
   qid  iter  docno  rel
giving TREC document numbers (docno, a string) and their relevance (rel, 
an integer) to query qid (a string).  iter string field is ignored.  
Fields are separated by whitespace, string fields can contain no whitespace.
File may contain no NULL characters.
*/

int 
get_prels (text_prels_file, all_trec_prels)
char *text_prels_file;
ALL_TREC_PRELS *all_trec_prels;
{
    int fd;
    int size = 0;
    char *trec_prels_buf;
    char *ptr;
    char *current_qid;
    char *qid_ptr, *docno_ptr, *rel_ptr, *iter_ptr;
    long i;
    long rel;
    int alg;
    TREC_PRELS *current_prels = NULL;

    /* Read entire file into memory */
    if (-1 == (fd = open (text_prels_file, 0)) ||
        -1 == (size = lseek (fd, 0L, 2)) ||
        NULL == (trec_prels_buf = malloc ((unsigned) size+2)) ||
        -1 == lseek (fd, 0L, 0) ||
        size != read (fd, trec_prels_buf, size) ||
	-1 == close (fd)) {

        set_error (SM_ILLPA_ERR, "Cannot read prels file", "trec_eval");
        return (UNDEF);
    }

    current_qid = "";

    /* Initialize all_trec_prels */
    all_trec_prels->num_q_prels = 0;
    all_trec_prels->max_num_q_prels = INIT_NUM_QUERIES;
    if (NULL == (all_trec_prels->trec_prels = tMalloc (INIT_NUM_QUERIES,
						     TREC_PRELS)))
	return (UNDEF);
    
    if (size == 0)
	return (0);

    /* Append ending newline if not present, Append NULL terminator */
    if (trec_prels_buf[size-1] != '\n') {
	trec_prels_buf[size] = '\n';
	size++;
    }
    trec_prels_buf[size] = '\0';

    ptr = trec_prels_buf;

    while (*ptr) {
	/* Get current line */
	/* Get qid */
	while (*ptr != '\n' && isspace (*ptr)) ptr++;
	qid_ptr = ptr;
	while (! isspace (*ptr)) ptr++;
	if (*ptr == '\n') {
	    set_error (SM_ILLPA_ERR, "Malformed prels line", "trec_eval");
	    return (UNDEF);
	}
	*ptr++ = '\0';
	/* Skip iter */
	while (*ptr != '\n' && isspace (*ptr)) ptr++;
	iter_ptr = ptr;
	while (! isspace (*ptr)) ptr++;
	if (*ptr == '\n') {
	    set_error (SM_ILLPA_ERR, "Malformed prels line", "trec_eval");
	    return (UNDEF);
	}
	*ptr++ = '\0';
	/* Get docno */
	while (*ptr != '\n' && isspace (*ptr)) ptr++;
	docno_ptr = ptr;
	while (! isspace (*ptr)) ptr++;
	if (*ptr == '\n') {
	    set_error (SM_ILLPA_ERR, "Malformed prels line", "trec_eval");
	    return (UNDEF);
	}
	*ptr++ = '\0';
	/* Get relevance */
	while (*ptr != '\n' && isspace (*ptr)) ptr++;
	if (*ptr == '\n') {
	    set_error (SM_ILLPA_ERR, "Malformed prels line", "trec_eval");
	    return (UNDEF);
	}
	rel_ptr = ptr;
	while (! isspace (*ptr)) ptr++;
	if (*ptr != '\n') {
	    *ptr++ = '\0';
	    while (*ptr != '\n' && isspace (*ptr)) ptr++;
	    if (*ptr != '\n') {
		set_error (SM_ILLPA_ERR, "malformed prels line",
			   "trec_eval");
		return (UNDEF);
	    }
	}
	*ptr++ = '\0';

	if (0 != strcmp (qid_ptr, current_qid)) {
	    /* Query has changed. Must check if new query or this is more
	       judgements for an old query */
	    for (i = 0; i < all_trec_prels->num_q_prels; i++) {
		if (0 == strcmp (qid_ptr, all_trec_prels->trec_prels[i].qid))
		    break;
	    }
	    if (i >= all_trec_prels->num_q_prels) {
		/* New unseen query, add and initialize it */
		if (all_trec_prels->num_q_prels >=
		    all_trec_prels->max_num_q_prels) {
		    all_trec_prels->max_num_q_prels *= 10;
		    if (NULL == (all_trec_prels->trec_prels = 
				 tRealloc (all_trec_prels->trec_prels,
					  all_trec_prels->max_num_q_prels,
					  TREC_PRELS)))
			return (UNDEF);
		}
		current_prels = &all_trec_prels->trec_prels[i];
		current_prels->qid = qid_ptr;
		current_prels->num_text_prels = 0;
		current_prels->max_num_text_prels = INIT_NUM_RELS;
		if (NULL == (current_prels->text_prels =
			     tMalloc (INIT_NUM_RELS, TEXT_PRELS)))
		    return (UNDEF);
		all_trec_prels->num_q_prels++;
	    }
	    else {
		/* Old query, just switch current_q_index */
		current_prels = &all_trec_prels->trec_prels[i];
	    }
	    current_qid = current_prels->qid;
	}
	
	/* Add judgement to current query's list */
	if (current_prels->num_text_prels >= 
	    current_prels->max_num_text_prels) {
	    /* Need more space */
	    current_prels->max_num_text_prels *= 10;
	    if (NULL == (current_prels->text_prels = 
			 tRealloc (current_prels->text_prels,
				  current_prels->max_num_text_prels,
				  TEXT_PRELS)))
		return (UNDEF);
	}
	current_prels->text_prels[current_prels->num_text_prels].docno =
		docno_ptr;
	current_prels->text_prels[current_prels->num_text_prels].rel =
	    atof(rel_ptr);
	current_prels->num_text_prels++;
    }

	all_trec_prels->ptr = trec_prels_buf;

    return (1);
}

int delete_prels (ALL_TREC_PRELS *all_trec_prels)
{
	int i;

	for (i=0; i<all_trec_prels->num_q_prels; i++)
		free(all_trec_prels->trec_prels[i].text_prels);
	free(all_trec_prels->trec_prels);
	free(all_trec_prels->ptr);
}
