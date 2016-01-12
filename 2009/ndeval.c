/*

Compute novelty and diversity evaluation measures.

Computes the following measures:
  1) alpha-nDCG (@5, 10 and 20) as defined by Clarke et al., SIGIR 2008
  2) an "intent aware" version of precision (@5, 10 and 20) as defined
     by Agrawal et al., WSDM 2009.

Evalution measures are written to standard output as a CSV file.
*/

char *help =
"ndeval [options] qrels run\n"
"  options:\n"
"    -alpha value\n"
"          Parameter for computing alpha-nDCG\n"
"    -traditional\n"
"        Sort runs by score and then by docno, which is the traditional\n"
"        behavior for TREC.  By default, the program sorts runs by rank.\n"
"    -c\n"
"        Average over the complete set of topics in the relevance judgements\n"
"        instead of the topics in the intersection of relevance judgements\n"
"        and results.\n"
;

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

#define DEPTH 20  /* depth for computing effectiveness measures */
#define ALPHA 0.5 /* default alpha value for alpha-nDCG */

static char *version = "version 1.3 (Fri 30 Oct 2009 09:39:18 EDT)";

static double alpha = ALPHA;  /* alpha value for alpha-nDCG */
static int traditional = 0;   /* use traditional TREC sort order for runs */

static char *programName = (char *) 0;

static void
error (char *format, ...)
{
  va_list args;

  fflush (stderr);
  if (programName)
    fprintf (stderr, "%s: ", programName);
  va_start (args, format);
  vfprintf (stderr, format, args);
  va_end (args);
  fflush (stderr);
  exit (1);
}

static void *
localMalloc (size_t size)
{
  void *memory;

  if ((memory = malloc (size)))
    return memory;
  else
    {
      error ("Out of memory!\n");
      /*NOTREACHED*/
      return (void *) 0;
    }
}

static void *
localRealloc (void *memory, size_t size)
{
  if ((memory = realloc (memory, size)))
    return memory;
  else
    {
      error ("Out of memory!\n");
      /*NOTREACHED*/
      return (void *) 0;
    }
}

static char *
localStrdup (const char *string)
{
  return strcpy (localMalloc (strlen (string) + 1), string);
}

static void
setProgramName (char *argv0)
{
  char *pn;

  if (argv0 == (char *) 0)
    {
      programName = (char *) 0;
      return;
    }

  for (pn = argv0 + strlen (argv0); pn > argv0; --pn)
    if (*pn == '/')
      {
        pn++;
        break;
      }

  programName = localStrdup (pn);
}

static char *
getline (FILE *fp)
{
  int const GETLINE_INITIAL_BUFSIZ = 256;
  static unsigned bufsiz = 0;
  static char *buffer = (char *) 0;
  unsigned count = 0;

  if (bufsiz == 0)
    {
      buffer = (char *) localMalloc ((unsigned) GETLINE_INITIAL_BUFSIZ);
      bufsiz = GETLINE_INITIAL_BUFSIZ;
    }

  if (fgets (buffer, bufsiz, fp) == NULL)
    return (char *) 0;

  for (;;)
    {
      unsigned nlpos = strlen (buffer + count) - 1;
      if (buffer[nlpos + count] == '\n')
        {
          if (nlpos && buffer[nlpos + count - 1] == '\r')
            --nlpos;
          buffer[nlpos + count] = '\0';
          return buffer;
        }
      count = bufsiz - 1;
      bufsiz <<= 1;
      buffer = (char *) localRealloc (buffer, (unsigned) bufsiz);
      if (fgets (buffer + count, count + 2, fp) == NULL)
        {
          buffer[count] = '\0';
          return buffer;
        }
    }
}

static int
split (char *s, char **a, int m)
{
  int n = 0;

  while (n < m)
    {
      for (; isspace (*s); s++)
        ;
      if (*s == '\0')
        return n;

      a[n++] = s;

      for (s++; *s && !isspace (*s); s++)
        ;
      if (*s == '\0')
        return n;

      *s++ = '\0';
    }

  return n;
}

static int
naturalNumber (char *s)
{
  int value = 0;
  const int LARGE_ENOUGH = 100000;

  if (s == (char *) 0 || *s == '\0')
    return -1;

  for (; *s; s++)
    if (*s >= '0' && *s <= '9')
      {
	if (value > LARGE_ENOUGH)
	  return -1;
	value = 10*value + (*s - '0');
      }
    else
      return -1;

  return value;
}

/* parseTopic:
    topic numbers in run files may be prefaced by a string indicating the task;
    remove this string (e.g., "wt09-") and extract the topic number;
    we assume the string ends with a "-" character;
*/
static int parseTopic(char *s)
{
  if (*s >= '0' && *s <= '9')
    return naturalNumber (s);

  for (;*s && *s != '-'; s++)
    ;
    
  return naturalNumber (s + 1);
}


struct result { /* a single result, with a pointer to relevance judgments */
  char *docno;
  int topic, rank, *rel;
  double score;  /* used only for traditional sort */
};

struct rList { /* result list (or summarized qrels) for a single topic  */
  struct result *list;
  int topic, subtopics, actualSubtopics, results;
  double precision[DEPTH], dcg[DEPTH];
};

struct qrel {  /* a single qrel */
  char *docno;
  int topic, subtopic, judgment;
};

/* qrelCompare:
    qsort comparison function for qrels
*/
static int
qrelCompare (const void *a, const void *b)
{
  struct qrel *aq = (struct qrel *) a;
  struct qrel *bq = (struct qrel *) b;

  if (aq->topic < bq->topic)
    return -1;
  if (aq->topic > bq->topic)
    return 1;
  return strcmp (aq->docno, bq->docno);
}

/* qrelSort:
    sort qrels by topic and then by docno
*/
static void
qrelSort (struct qrel *q, int n)
{
  qsort (q, n, sizeof (struct qrel), qrelCompare);
}

/* qrelCountTopics:
    count the number of distinct topics in a qrel file; assumes qrels are sorted
*/
static int
qrelCountTopics (struct qrel *q, int n)
{
  int i, topics = 1, currentTopic = q[0].topic;

  for (i = 1; i < n; i++)
    if (q[i].topic != currentTopic)
      {
	topics++;
	currentTopic = q[i].topic;
      }

  return topics;
}

/* computeActualSubtopics:
    for a given qrel result list, determine the number of subtopics actually
    represented; if a subtopic has never received a positive judgment, we
    ignore it.
*/
static void
computeActualSubtopics (struct rList *rl)
{
  int i,j;
  int *rel = (int *) alloca (rl->subtopics*sizeof(int));

  for (i = 0; i < rl->subtopics; i++)
    rel[i] = 0;

  for (i = 0; i < rl->results; i++)
    for (j = 0; j < rl->subtopics; j++)
      rel[j] |= rl->list[i].rel[j];

  rl->actualSubtopics = 0;
  for (i = 0; i < rl->subtopics; i++)
    if (rel[i])
      rl->actualSubtopics++;
}

/* idealResult:
    for a qrel result list, assign ranks to maximize gain at each rank;
    the problem is NP-complete, but a simple greedy algorithm works fine
*/
static void
idealResult (struct rList *rl)
{
  int i, rank;
  double *subtopicGain = (double *) alloca (rl->subtopics*sizeof(double));

  for (i = 0; i < rl->subtopics; i++)
    subtopicGain[i] = 1.0;

  for (i = 0; i < rl->results; i++)
    rl->list[i].rank = 0;

  /* horrible quadratic greedy approximation of the ideal result */
  for (rank = 1; rank <= rl->results; rank++)
    {
      int where = -1;
      double maxScore = 0.0; 

      for (i = 0; i < rl->results; i++)
	if (rl->list[i].rank == 0)
	  {
	    int j;
	    double currentScore = 0.0;

	    for (j = 0; j < rl->subtopics; j++)
	      if (rl->list[i].rel[j])
		currentScore += subtopicGain[j];

	    /* tied scores are arbitrarily resolved by docno */
	    if (
	      where == -1
	      || currentScore > maxScore
	      || (
		currentScore == maxScore
		&& strcmp (rl->list[i].docno, rl->list[where].docno) > 0
	      )
	    )
	      {
		maxScore = currentScore;
		where = i;
	      }
	  }

      rl->list[where].rank = rank;

      for (i = 0; i < rl->subtopics; i++)
	if (rl->list[where].rel[i])
	  subtopicGain[i] *= (1.0 - alpha);
    }
}

/* resultCompareByRank:
    qsort comparison funtion for results; sort by topic and then by rank
*/
resultCompareByRank (const void *a, const void *b)
{
  struct result *ar = (struct result *) a;
  struct result *br = (struct result *) b;
  if (ar->topic < br->topic)
    return -1;
  if (ar->topic > br->topic)
    return 1;
  return ar->rank - br->rank;
}

/* resultSortByRank:
    sort results, first by topic and then by rank
*/
static void
resultSortByRank (struct result *list, int results)
{
  qsort (list, results, sizeof (struct result), resultCompareByRank);
}

/* resultCompareByDocno:
    qsort comparison funtion for results; sort by topic and then by docno
*/
static int
resultCompareByDocno (const void *a, const void *b)
{
  struct result *ar = (struct result *) a;
  struct result *br = (struct result *) b;
  if (ar->topic < br->topic)
    return -1;
  if (ar->topic > br->topic)
    return 1;
  return strcmp(ar->docno, br->docno);
}

/* resultSortByRank:
    sort results, first by topic and then by docno
*/
static void
resultSortByDocno (struct result *list, int results)
{
  qsort (list, results, sizeof (struct result), resultCompareByDocno);
}

/* resultCompareByScore:
    qsort comparison funtion for results; sort by topic, then by score, and
    then by docno, which is the traditional sort order for TREC runs
*/
resultCompareByScore (const void *a, const void *b)
{
  struct result *ar = (struct result *) a;
  struct result *br = (struct result *) b;
  if (ar->topic < br->topic)
    return -1;
  if (ar->topic > br->topic)
    return 1;
  if (ar->score < br->score)
    return 1;
  if (ar->score > br->score)
    return -1;
  return strcmp (ar->docno, br->docno);
}

/* resultSortByScore:
    sort results; first by topic, then by score, and then by docno
*/
static void
resultSortByScore (struct result *list, int results)
{
  qsort (list, results, sizeof (struct result), resultCompareByScore);
}

/* computeDCG:
    computer DCG for a result list (a run or qrels);
    assumes all results are labeled with relevance judgments
*/
static void
computeDCG (struct rList *rl)
{
  int i;
  double *subtopicGain = (double *) alloca (rl->subtopics*sizeof(double));

  for (i = 0; i < rl->subtopics; i++)
    subtopicGain[i] = 1.0;

  for (i = 0; i < DEPTH; i++)
    rl->dcg[i] = 0.0;

  for (i = 0; i < DEPTH && i < rl->results; i++)
    {
      int j;
      double score = 0.0;

      if (rl->list[i].rel)
	for (j = 0; j < rl->subtopics; j++)
	  if (rl->list[i].rel[j])
	    {
	      score += subtopicGain[j];
	      subtopicGain[j] *= (1.0 - alpha);
	    }
      rl->dcg[i] = score*log(2.0)/log(i + 2.0); /* discounted gain */
    }

  for (i = 1; i < DEPTH; i++)
    rl->dcg[i] += rl->dcg[i-1];  /* cumulative gain */
}

/* computePrecision:
    computer intent aware precision for a result list;
    assumes all results are labeled with relevance judgments
*/
static void
computePrecision (struct rList *rl)
{
  int i, count = 0;

  if (rl->actualSubtopics == 0)
    return;

  for (i = 0; i < DEPTH && i < rl->results; i++)
    {
      if (rl->list[i].rel)
	{
	  int j;

	  for (j = 0; j < rl->subtopics; j++)
	    if (rl->list[i].rel[j])
	      count++;
	}
      rl->precision[i] = ((double) count)/((i + 1)*rl->actualSubtopics);
    }

  for (; i < DEPTH; i++)
    rl->precision[i] = ((double) count)/((i + 1)*rl->actualSubtopics);
}

/* qrelPopulateResultList:
    populate an ideal result list from the qrels;
    also used to label the run with relevance judgments
*/
static void
qrelPopulateResultList (struct qrel *q, int n, struct rList *rl, int topics)
{
  int i, j, k, currentTopic;
  char *currentDocno = "";

  j = currentTopic = -1;
  for (i = 0; i < n; i++)
    {
      if (q[i].topic != currentTopic)
	{
	  j++;
	  currentTopic = q[i].topic;
	  currentDocno = "";
	  rl[j].topic = currentTopic;
	  rl[j].subtopics = rl[j].results = 0;
	  for (k = 0; k < DEPTH; k++)
	    rl[j].precision[k] = rl[j].dcg[k] = 0.0;
	}
      if (rl[j].subtopics <= q[i].subtopic)
	rl[j].subtopics = q[i].subtopic + 1;
      if (strcmp (q[i].docno, currentDocno) != 0)
	{
	  currentDocno = q[i].docno;
	  rl[j].results++;
	}
    }

  for (i = 0; i < topics; i++)
    {
      rl[i].list =
	(struct result *) localMalloc (rl[i].results*sizeof (struct result));
      for (j = 0; j < rl[i].results; j++)
	{
	  rl[i].list[j].topic = rl[i].topic;
	  rl[i].list[j].rel =
	    (int *) localMalloc (rl[i].subtopics*sizeof (int));
	  for (k = 0; k < rl[i].subtopics; k++)
	    rl[i].list[j].rel[k] = 0;
	}
    }

  j = k = currentTopic = -1;
  currentDocno = "";
  for (i = 0; i < n; i++)
    {
      if (q[i].topic != currentTopic)
	{
	  j++;
	  currentTopic = q[i].topic;
	  k = -1;
	  currentDocno = "";
	}
      if (strcmp (q[i].docno, currentDocno) != 0)
	{
	  currentDocno = q[i].docno;
	  k++;
	  rl[j].list[k].docno = localStrdup (currentDocno);
	}
      rl[j].list[k].rel[q[i].subtopic] = q[i].judgment;
    }

  for (i = 0; i < topics; i++)
    {
      computeActualSubtopics (rl + i);
      idealResult (rl + i);
      resultSortByRank (rl[i].list, rl[i].results);
      computeDCG (rl + i);
      resultSortByDocno (rl[i].list, rl[i].results);
    }
}

/* qrelToRList:
    construct an ideal result list from the qrels
*/
static struct rList *
qrelToRList (struct qrel *q, int n, int *topics)
{
  int i;
  struct rList *rl;

  *topics = qrelCountTopics (q, n);
  rl = (struct rList *) localMalloc ((*topics)*sizeof (struct rList));
  qrelPopulateResultList (q, n, rl, *topics);

  return rl;
}

/* qrelToRList:
    process the qrels file, contructing an ideal result list
*/
static struct rList *
processQrels (char *fileName, int *topics)
{
  FILE *fp;
  char *line;
  struct qrel *q;
  int i = 0, n = 0;

  if ((fp = fopen (fileName, "r")) == NULL)
    error ("cannot open qrel file \"%s\"\n", fileName);

  while (getline(fp))
    n++;

  fclose (fp);

  if (n == 0)
    error ("qrel file \"%s\" is empty\n", fileName);

  q = localMalloc (n*sizeof (struct qrel));

  if ((fp = fopen (fileName, "r")) == NULL)
    error ("cannot open qrel file \"%s\"\n", fileName);

  while (line = getline (fp))
    {
      char *a[4];
      int topic, subtopic, judgment;

      if (
	split (line, a, 4) != 4
	|| (topic = naturalNumber (a[0])) < 0
	|| (subtopic = naturalNumber (a[1])) < 0
	|| (judgment = naturalNumber (a[3])) < 0
      )
	error (
	  "syntax error in qrel file \"%s\" at line %d\n", fileName, i + 1
	);
      else
	{
	  q[i].topic = topic;
	  q[i].subtopic = subtopic;
	  q[i].judgment = judgment;
	  q[i].docno = localStrdup (a[2]);
	  i++;
	}
    }

  fclose (fp);

  qrelSort (q, n);

  return qrelToRList (q, n, topics);
}

/* resultCountTopics:
    count the number of distinct topics in a run; assumes results are sorted
*/
static int
resultCountTopics (struct result *r, int n)
{
  int i, topics = 1, currentTopic = r[0].topic;

  for (i = 1; i < n; i++)
    if (r[i].topic != currentTopic)
      {
	topics++;
	currentTopic = r[i].topic;
      }

  return topics;
}


/* populateResultList:
    populate a result list from a run;
*/
static void
populateResultList (struct result *r, int n, struct rList *rl, int topics)
{
  int i, j, k, currentTopic = -1;

  j = 0;
  for (i = 0; i < n; i++)
    if (r[i].topic != currentTopic)
      {
	currentTopic = r[i].topic;
	rl[j].list = r + i;
	rl[j].topic = currentTopic;
	rl[j].subtopics = 0;
	rl[j].actualSubtopics = 0;
	if (j > 0)
	  rl[j-1].results = rl[j].list - rl[j-1].list;
	for (k = 0; k < DEPTH; k++)
	  rl[j].precision[k] = rl[j].dcg[k] = 0.0;
	j++;
      }
  if (j > 0)
    rl[j-1].results = (r + n) - rl[j-1].list;
}

/* forceTraditionalRanks:
    Re-assign ranks so that runs are sorted by score and then by docno,
    which is the traditional sort order for TREC runs.
*/
static void
forceTraditionalRanks (struct result *r, int n)
{
  int i, rank, currentTopic = -1;

  resultSortByScore (r, n);

  for (i = 0; i < n; i++)
    {
      if (r[i].topic != currentTopic)
	{
	  currentTopic = r[i].topic;
	  rank = 1;
	}
      r[i].rank = rank;
      rank++;
    }
}


/* processRun:
    process a run file, returning a result list
*/
static struct rList *
processRun (char *fileName, int *topics, char **runid)
{
  FILE *fp;
  char *line;
  int i = 0, n = 0;
  int needRunid = 1;
  struct result *r;
  struct rList *rl;

  if ((fp = fopen (fileName, "r")) == NULL)
    error ("cannot open run file \"%s\"n", fileName);

  while (getline(fp))
    n++;

  fclose (fp);

  if (n == 0)
    error ("run file \"%s\" is empty\n", fileName);

  r = localMalloc (n*sizeof (struct result));

  if ((fp = fopen (fileName, "r")) == NULL)
    error ("cannot open run file \"%s\"\n", fileName);

  while (line = getline (fp))
    {
      char *a[6];
      int topic, rank;

      if (
	split (line, a, 6) != 6
	|| (topic = parseTopic (a[0])) < 0
	|| (rank = naturalNumber (a[3])) < 0
      )
	error ("syntax error in run file \"%s\" at line %d\n", fileName, i + 1);
      else
	{
	  if (needRunid)
	    {
	      *runid = localStrdup (a[5]);
	      needRunid = 0;
	    }
	  r[i].docno = localStrdup (a[2]);
	  r[i].topic = topic;
	  r[i].rank = rank;
	  r[i].rel = (int *) 0;
	  if (traditional)
	    sscanf (a[4],"%lf", &(r[i].score));
	  else
	    r[i].score = 0.0;
	  i++;
	}
  }

  /* force ranks to be consistent with traditional TREC sort order */
  if (traditional)
    forceTraditionalRanks (r, n);

  /* for each topic, verify that ranks have not been duplicated */
  resultSortByRank (r, n);
  for (i = 1; i < n; i++)
    if (r[i].topic == r[i-1].topic && r[i].rank == r[i-1].rank)
      error (
	"duplicate rank (%d) for topic %d in run file \"%s\"\n",
	r[i].rank, r[i].topic, fileName
      );

  /* for each topic, verify that docnos have not been duplicated */
  resultSortByDocno (r, n);
  for (i = 1; i < n; i++)
    if (r[i].topic == r[i-1].topic && strcmp(r[i].docno,r[i-1].docno) == 0)
      error (
	"duplicate docno (%s) for topic %d in run file \"%s\"\n",
	r[i].docno, r[i].topic, fileName
      );

  /* split results by topic */
  *topics = resultCountTopics (r, n);
  rl = (struct rList *) localMalloc ((*topics)*sizeof (struct rList));
  populateResultList (r, n, rl, *topics);

  return rl;
}

/* applyJudgments:
    copy relevance judgments from qrel results to run results;
    assumes results are sorted by docno
*/
static void
applyJudgments (struct result *q, int qResults, struct result *r, int rResults)
{
  int i = 0, j = 0;

  while (i < qResults && j < rResults)
    {
      int cmp = strcmp (q[i].docno, r[j].docno);
      
      if (cmp < 0)
	  i++;
      else if (cmp > 0)
	  j++;
      else
	{
	  r[j].rel = q[i].rel;
	  i++;
	  j++;
	}
    }
}

/* normalizeDCG:
    normalize a result list against an ideal result list (created from qrels)
*/
static void
normalizeDCG (struct rList *ql, struct rList *rl)
{
  int i;

  for (i = 0; i < DEPTH; i++)
    if (rl->dcg[i])
      rl->dcg[i] /= ql->dcg[i]; /* if rl->dcg[i] is non-zero, so is ql->dcg[i]*/
}

/* applyQrels:
    transfer relevance judgments from qrels to a run
*/
static int
applyQrels (struct rList *qrl, int qTopics, struct rList *rrl, int rTopics)
{
  int actualTopics = 0, i = 0, j = 0;

  while (i < qTopics && j < rTopics)
    if (qrl[i].topic < rrl[j].topic)
      i++;
    else if (qrl[i].topic > rrl[j].topic)
      j++;
    else
      {
	rrl[j].subtopics = qrl[i].subtopics;
	rrl[j].actualSubtopics = qrl[i].actualSubtopics;
	applyJudgments (
	  qrl[i].list, qrl[i].results, rrl[j].list, rrl[j].results
	);
	resultSortByRank (rrl[j].list, rrl[j].results);
	computeDCG (rrl + j);
	normalizeDCG (qrl + i, rrl + j);
	computePrecision (rrl + j);
	i++;
	j++;
	actualTopics++;
      }

  return actualTopics;
}

/* outputMeasures:
    output evaluation measures as a CSV file
*/
static void
outputMeasures (struct rList *rl, int topics, int actualTopics, char *runid)
{
  int i;
  double totalDCG5 = 0.0, totalDCG10 = 0.0, totalDCG20 = 0.0;
  double totalP5 = 0.0, totalP10 = 0.0, totalP20 = 0.0;

  printf ("runid,topic,alpha-ndcg@5,alpha-ndcg@10,alpha-ndcg@20,");
  printf ("IA-P@5,IA-P@10,IA-P@20\n");
  if (actualTopics == 0)
    {
      printf ("%s,amean,0.00,0.00,0.00,0.00,0.00,0.00\n", runid);
      return;
    }

  for (i = 0; i < topics; i++)
    {
      printf (
	"%s,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n",
	runid, rl[i].topic, rl[i].dcg[4], rl[i].dcg[9], rl[i].dcg[19],
	rl[i].precision[4], rl[i].precision[9], rl[i].precision[19]
      );
      totalDCG5 += rl[i].dcg[4];
      totalDCG10 += rl[i].dcg[9];
      totalDCG20 += rl[i].dcg[19];
      totalP5 += rl[i].precision[4];
      totalP10 += rl[i].precision[9];
      totalP20 += rl[i].precision[19];
    }
  printf (
    "%s,amean,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n",
    runid,
    totalDCG5/actualTopics, totalDCG10/actualTopics, totalDCG20/actualTopics,
    totalP5/actualTopics, totalP10/actualTopics, totalP20/actualTopics
  );
}

static void
usage ()
{
  error (
    "Usage: %s [options] qrels run   (-help for full usage information)\n",
    programName
  );
}

int
main (int argc, char **argv)
{
  char *runid;
  int qTopics, rTopics, actualTopics;
  struct rList *qrl, *rrl;
  int cFlag = 0;         /* average over complete set of queries */

  setProgramName (argv[0]);

  while (argc != 3)
    if (argc >= 2 && strcmp ("-version", argv[1]) == 0)
      {
	printf ("%s: %s\n", programName, version);
	exit (0);
      }
    else if (argc >= 2 && strcmp ("-help", argv[1]) == 0)
      {
	printf (help);
	exit (0);
      }
    else if (argc >= 5 && strcmp ("-alpha", argv[1]) == 0)
      {
	sscanf (argv[2], "%lf", &alpha);
	if (alpha < 0.0 || alpha > 1.0)
	  usage();
	argc -= 2;
	argv += 2;
      }
    else if (argc >= 4 && strcmp ("-traditional", argv[1]) == 0)
      {
	traditional = 1;
	--argc;
	argv++;
      }
    else if (argc >= 4 && strcmp ("-c", argv[1]) == 0)
      {
	cFlag = 1;
	--argc;
	argv++;
      }
    else
      usage();

  qrl = processQrels (argv[1], &qTopics);
  rrl = processRun (argv[2], &rTopics, &runid);
  actualTopics = applyQrels (qrl, qTopics, rrl, rTopics);
  if (cFlag)
    actualTopics = qTopics;
  outputMeasures (rrl, rTopics, actualTopics, runid);

  return 0;
}
