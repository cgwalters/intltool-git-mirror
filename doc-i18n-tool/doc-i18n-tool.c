/*
 * doc-i18n-tool.c : load an XML or SGML DocBook document and extract text strings from
 * the document content.
 *
 * See Copyright for the status of this software.
 *
 * daniel@veillard.com
 */



#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <libintl.h>
#include <popt.h>
#include <libxml/xmlversion.h>
#include <libxml/xmlmemory.h>
#include <locale.h>

#ifdef LIBXML_DOCB_ENABLED
#include <libxml/DOCBparser.h>
#endif
#include <libxml/tree.h>
#include <libxml/debugXML.h>

#ifdef LIBXML_DOCB_ENABLED
static int sgml = 0;
static char *encoding = NULL;
#endif
static int noent = 0;
static int output_pot_file = 0;
static char *package = NULL;
static char *localedir = ".";
static char *alternate_file_name = NULL;
struct poptOption options [] =
  {
#ifdef LIBXML_DOCB_ENABLED
    { "sgml", 's', POPT_ARG_NONE, &sgml, 0, "parse as docbook sgml", "SGML" },
    { "encoding", 'e', POPT_ARG_STRING, &encoding, 0, "encoding for sgml files", "ENCODING" },
#endif
    { "noent", 'n', POPT_ARG_NONE, &noent, 0, "ignore entities in document", "NOENT" },
    { "output-pot-file", 'p', POPT_ARG_NONE, &output_pot_file, 0, "output a pot file for the document", "OUTPUT_POT_FILE" },
    { "package", 'g', POPT_ARG_STRING, &package, 0, "output a pot file for the document", "PACKAGE" },
    { "localedir", 'l', POPT_ARG_STRING, &localedir, 0, "localedir for the document", "LOCALEDIR" },
    { "filename", 'f', POPT_ARG_STRING, &alternate_file_name, 0, "alternate filename", "FILENAME" },
    { NULL, '\0', 0, NULL, 0}
  };
 
/*
 * Null terminated list of elements which should be considered
 * presentational and should not break strings.
 *
 * Add elements names as needed.
 */
const char *presentation_elements[] = {
    "accel",
    NULL
};


xmlChar *
processString (const xmlChar *value,
	       const char    *filename, 
	       int            line_no)
{
  if (output_pot_file)
    {
      const char *ptr;
      char *ptr2;
      char *new_line;
      int new_len = 0;
      
      for (ptr = value; *ptr != '\000'; ptr++)
	{
	  if (*ptr == '\n')
	    new_len += 5;
	  else if (*ptr == '\"')
	    new_len += 2;
	  else
	    new_len++;
	}
      new_line = malloc (sizeof (char) *(new_len + 1));
      for (ptr = value, ptr2 = new_line; *ptr != '\000'; ptr++)
	{
	  if (*ptr == '\n')
	    {
	      *ptr2++ = '\\';
	      *ptr2++ = 'n';
	      *ptr2++ = '\"';
	      *ptr2++ = '\n';
	      *ptr2++ = '\"';
	    }
	  else if (*ptr == '\"')
	    {
	      *ptr2++ = '\\';
	      *ptr2++ = '\"';
	    }
	  else
	    *ptr2++ = *ptr;
	}
      *ptr2 = '\000';
      if (alternate_file_name)
	printf ("#: %s:%d\n", alternate_file_name, line_no);
      else
	printf ("#: %s:%d\n", filename, line_no);
      printf ("msgid \"%s\"\n", new_line);
      printf ("msgstr \"\"\n\n");
      free (new_line);
    }
  else
    {
      return xmlStrdup (gettext (value));
    }
  return NULL;
}

static void
processNode (xmlNodePtr  node,
	     const char *filename,
	     int         line)
{
  if (node->type == XML_TEXT_NODE)
    {
      xmlChar *res;

      res = processString (node->content, filename, line);

      if (res != NULL)
	{
	  xmlFree (node->content);
	  node->content = res;
	}
    }
  else if (node->type == XML_ELEMENT_NODE)
    {
      int pres;
      xmlNodePtr children = node->children;
      const xmlChar *name;
      const char **tst;

      if (children == NULL)
	  return;

      pres = 1;
      /* check to see if all children are only text or presentation elements */
      while ((children != NULL) && (pres != 0)) {
	  if (children->type == XML_ELEMENT_NODE) {
	      name = children->name;
	      tst = presentation_elements;
	      while (*tst != NULL) {
		  if (!xmlStrcmp((const xmlChar *)*tst, name))
		      break;
		  tst++;
	      }
	      if (*tst == NULL) {
		  pres = 0;
		  break;
	      }
	  }
	  children = children->next;
      }
      if (pres == 1) {
	  xmlChar *content;

	  /*
	   * get the value, scrap the node list, replace with 
	   * the modified text content. Let the tree walking
	   * process the text node at the next step.
	   */
	  content = xmlNodeGetContent(node);
	  children = node->children;
	  node->last = node->children = NULL;
	  xmlFreeNodeList(children);
	  children = xmlNewDocText(node->doc, content);
	  xmlAddChild(node, children);
	  xmlFree(content);
      }
    }
}

static void
processSubtree (xmlNodePtr  node,
		const char *filename)
{
  xmlNodePtr cur;
  int line = 0;

  if (node == NULL)
    return;

  cur = node;
  while (cur != NULL)
    {
      if (node->type == XML_ELEMENT_NODE) {
	line = (int) node->content;
      }
      processNode (cur, filename, line);

      /*
       * skip to next node
       */
      if (cur->type == XML_ENTITY_REF_NODE)
	{
	  processSubtree (cur->children, filename);
	}
      if ((cur->children != NULL) && (cur->type != XML_ENTITY_REF_NODE))
	{
	  cur = cur->children;
	}
      else if (cur->type == XML_ENTITY_DECL)
	{
	  return;
	}
      else if (cur->next != NULL)
	{
	  cur = cur->next;
	}
      else
	{
	  do
	    {
	      cur = cur->parent;
	      if (cur == NULL)
		break;
	      if (cur->next != NULL) {
		cur = cur->next;
		break;
	      }
	    } while ((cur != NULL) && (cur != node));
	}
      if (cur == node)
	return;
    }
}

static void
output_pot_header (const char *filename)
{
  printf ("# SOME DESCRIPTIVE TITLE\n");
  printf ("# Copyright (C) YEAR Free Software Foundation, Inc.\n");
  printf ("# FIRST AUTHOR <EMAIL@ADDRESS>, YEAR.\n");
  if (alternate_file_name)
    printf ("#\n# %s\n\n", alternate_file_name);
  else
    printf ("#\n# %s\n\n", filename);
}

static void
processDocument (xmlDocPtr   doc,
		 const char *filename)
{
  xmlNodePtr cur;

  if (doc == NULL)
    return;

  if (output_pot_file)
    output_pot_header (filename);
  cur = xmlDocGetRootElement (doc);
  processSubtree (cur, filename);
}

static void
processFile (const char *filename)
{
  xmlDocPtr doc;

#ifdef LIBXML_DOCB_ENABLED
  if (sgml)
    doc = docbParseFile (filename, encoding);
  else
    doc = xmlParseFile (filename);
#else
  doc = xmlParseFile (filename);
#endif
  processDocument (doc, filename);
  if (!output_pot_file)
    xmlSaveFile ("-", doc);
  xmlFreeDoc (doc);
}

int main (int argc, char **argv)
{
  poptContext context;
  const char **files;
  const char *file_name;

  context = poptGetContext ("doc-i18n-tool", argc, (const char**)argv, options, 0);
  while (poptGetNextOpt (context) != -1);

  setlocale (LC_ALL, "");
  if (!output_pot_file)
    {
      bindtextdomain (package, localedir);
      bind_textdomain_codeset (package, "UTF-8");
      textdomain (package);
    }
  xmlLineNumbersDefault (1);
  if (noent == 0) xmlSubstituteEntitiesDefault(1);

  files = poptGetArgs (context);
  if (files == NULL)
    exit (0);

  while ((file_name = *files++) != NULL)
    {
      struct stat s;
      if ((stat (file_name, &s) < 0) ||
	  (! S_ISREG (s.st_mode)))
	{
	  printf ("File %s doesn't exist\n", file_name);
	  exit (1);
	}
      processFile (file_name);
    }

  poptFreeContext (context);
  xmlCleanupParser ();
  xmlMemoryDump ();

  return 0;
}
