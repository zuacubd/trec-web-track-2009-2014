Although TREC 2012 web track runs were submitted as either "adhoc" or
"diversity" task runs, we evaluated all runs under both the adhoc
and diversity conditions.  Adhoc evaluation results are computed
using trec_eval and gdeval.pl (which computes NDCG and ERR at cutoff
rank 20).  Diversity results are computed using ndeval.c.  

Both ndeval.c and gdeval.pl are posted on this site.  The full topic
text is available as well.  Each topic has an adhoc interpretation
(as given by the "description" field) and several subtopic interpretations.
By construction, the "description" field of the topic is identical to
the first sub-topic.  For the adhoc condition, a document must be
relevant to the adhoc interpretation.  For the diversity condition,
documents must be relevant to one or more subtopics.

There are three relevance judgment ("qrels") files posted here.  The main
qrels file is qrels.diversity; the other two are derived from it.
The qrels.diversity qrels file (ab)uses the traditional TREC qrels format,
using the second field to indicate the subtopic the judgment is with
respect to.  If the second field is 'n',  the relevance judgment is
for subtopic 'n' where the subtopic numbers correspond to those in the
full topic file.  The judgment itself can be one of the following values:
 -2: spam or otherwise seems useless for any information need
  0: not relevant
  1: relevant
  2: highly relevant
  3: key (page or site is comprehensive and should be a top search
     result)
  4: nav (page is a navigational result for the query; query meant "go
     here")
By definition, if a document is ever marked as spam, it is spam for all subtopics.

The qrels.adhoc file is the qrels.diversity file restricted to just
the first subtopic (remember this is identical to the description by
construction) and with the second field changed back to '0'.  The
qrels-for-ndeval file is the qrels.diversity qrels with all spam judgments
changed to not relevant (ndeval.c complains about negative judgments) and
with eight subtopics removed:
    topic 167, subtopic 5;
    topic 170, subtopic 2;
    topic 174, subtopic 2;
    topic 177, subtopic 4;
    topic 183, subtopic 3;
    topic 187, subtopic 3;
    topic 190, subtopic 3;
    topic 195, subtopic 4.
These subtopics were removed because the have no known relevant documents
in the collection.
