// hijacked get_qrels.c for this

#include "common.h"
#include "sysfunc.h"
#include "smart_error.h"
#include "trec_eval.h"
#include <ctype.h>


int get_judgment_list (char *fname, judgment_list *list)
{
    int fd;
    int size = 0;
    char *trec_qrels_buf;
    char *ptr;
    char *current_qid;
    char *qid_ptr, *docno_ptr, *rel_ptr;
    long i;
    long rel;
    TREC_QRELS *current_qrels = NULL;

    /* Read entire file into memory */
    if (-1 == (fd = open (fname, 0)) ||
        -1 == (size = lseek (fd, 0L, 2)) ||
        NULL == (trec_qrels_buf = malloc ((unsigned) size+2)) ||
        -1 == lseek (fd, 0L, 0) ||
        size != read (fd, trec_qrels_buf, size) ||
	-1 == close (fd)) {

        set_error (SM_ILLPA_ERR, "Cannot read qrels file", "trec_eval");
        return (UNDEF);
    }

    current_qid = "";

    /* Initialize all_trec_qrels */
    list->num_q_qrels = 0;
    list->max_num_q_qrels = INIT_NUM_QUERIES;
    if (NULL == (list->trec_qrels = tMalloc (INIT_NUM_QUERIES,
						     TREC_QRELS)))
	return (UNDEF);
    
    if (size == 0)
	return (0);

    /* Append ending newline if not present, Append NULL terminator */
    if (trec_qrels_buf[size-1] != '\n') {
	trec_qrels_buf[size] = '\n';
	size++;
    }
    trec_qrels_buf[size] = '\0';

    ptr = trec_qrels_buf;

    while (*ptr) {
	/* Get current line */
	/* Get qid */
	while (*ptr != '\n' && isspace (*ptr)) ptr++;
	qid_ptr = ptr;
	while (! isspace (*ptr)) ptr++;
	if (*ptr == '\n') {
	    set_error (SM_ILLPA_ERR, "Malformed qrels line", "trec_eval");
	    return (UNDEF);
	}
	*ptr++ = '\0';
	/* Get docno */
	while (*ptr != '\n' && isspace (*ptr)) ptr++;
	docno_ptr = ptr;
	while (! isspace (*ptr)) ptr++;
	if (*ptr == '\n') {
	    set_error (SM_ILLPA_ERR, "Malformed qrels line", "trec_eval");
	    return (UNDEF);
	}
	*ptr++ = '\0';
	/* move on to end of line */
	while (*ptr != '\n') ptr++;
	*ptr++ = '\0';

	if (0 != strcmp (qid_ptr, current_qid)) {
	    /* Query has changed. Must check if new query or this is more
	       judgements for an old query */
	    for (i = 0; i < list->num_q_qrels; i++) {
		if (0 == strcmp (qid_ptr, list->trec_qrels[i].qid))
		    break;
	    }
	    if (i >= list->num_q_qrels) {
		/* New unseen query, add and initialize it */
		if (list->num_q_qrels >=
		    list->max_num_q_qrels) {
		    list->max_num_q_qrels *= 10;
		    if (NULL == (list->trec_qrels = 
				 tRealloc (list->trec_qrels,
					  list->max_num_q_qrels,
					  TREC_QRELS)))
			return (UNDEF);
		}
		current_qrels = &list->trec_qrels[i];
		current_qrels->qid = qid_ptr;
		current_qrels->num_text_qrels = 0;
		current_qrels->max_num_text_qrels = INIT_NUM_RELS;
		if (NULL == (current_qrels->text_qrels =
			     tMalloc (INIT_NUM_RELS, TEXT_QRELS)))
		    return (UNDEF);
		list->num_q_qrels++;
	    }
	    else {
		/* Old query, just switch current_q_index */
		current_qrels = &list->trec_qrels[i];
	    }
	    current_qid = current_qrels->qid;
	}
	
	/* Add judgement to current query's list */
	if (current_qrels->num_text_qrels >= 
	    current_qrels->max_num_text_qrels) {
	    /* Need more space */
	    current_qrels->max_num_text_qrels *= 10;
	    if (NULL == (current_qrels->text_qrels = 
			 tRealloc (current_qrels->text_qrels,
				  current_qrels->max_num_text_qrels,
				  TEXT_QRELS)))
		return (UNDEF);
	}
	current_qrels->text_qrels[current_qrels->num_text_qrels].docno =
		docno_ptr;
	current_qrels->text_qrels[current_qrels->num_text_qrels++].rel = 0;
    }

	list->ptr = trec_qrels_buf;

    return (1);
}
