Two evaluation programs, ndeval.c and gdeval.pl, are 
posted on the web site.  In the TREC 2011 track,
each topic has an adhoc interpretation (as given by the
"description" field) and several subtopic interpretations.
By construction, the "description" field of the topic is
identical to the first sub-topic.
For the adhoc condition, a document must have been
relevant to the adhoc interpretation.  For the diversity condition,
documents must have been relevant to one or more subtopics.

There are three relevance judgment ("qrels") files also posted
to the web site.  The main qrels file is qrels.diversity;
the other two are derived from it.  The qrels.diversity qrels file
abuses the traditional qrels format by using the second field to
indicate the subtopic the judgment is with respect to.
If the second field is 'n',  the relevance judgment 
is for subtopic 'n' where the subtopic numbers correspond to those
in the full topic file.  The judgment itself can be one
of the following values:
 -2: spam or otherwise seems useless for any information need
  0: not relevant
  1: relevant
  2: key (page or site is comprehensive and should be a top search
     result)
  3: nav (page is a navigational result for the query; query meant "go
     here")
By definition, if a document is ever marked as spam, it is spam
for all subtopics.

The qrels.adhoc file is the qrels.diversity file restricted
to just the first subtopic (by construction, this is identical to the
description field of the full topic statement) and with the second field
changed back to '0'.  The qrels-for-ndeval file is
the qrels.diversity qrels with all spam judgments changed to
not relevant (ndeval.c complains about negative judgments) and
with four subtopics removed: topic 109 subtopic 2; topic 134 subtopic 2;
subtopic 138 subtopic 4, and topic 140 subtopic 4.  These subtopics
were removed because the have no known relevant documents
in the collection.


