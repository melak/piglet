#include <sys/stat.h>	/* for mkdir() */
#include <sys/types.h>	/* for mkdir() */
#include <stdio.h>
#include <math.h>
#include <string.h>	/* for strnlen... */
#include <ctype.h> 	/* for toupper() */
#include <unistd.h>     /* for access() */

#include "db.h"		/* hierarchical database routines */
#include "eprintf.h"	/* error reporting functions */
#include "rlgetc.h"
#include "token.h"
#include "xwin.h"       /* for snapxy() */
#include "readfont.h"	/* for writestring() */
#include "rubber.h"
#include "stack.h"	/* for generating $FILE arguments in archives */
#include "rlgetc.h"

#define EPS 1e-6

#define CELL 0		/* modes for db_def_print() */
#define ARCHIVE 1
#define BB 2		/* draw bounding box and don't do xform */


char *PATH=".:./.pigrc:~/.pigrc:/usr/local/lib/piglet:/usr/lib/piglet";


/* master symbol table pointers */
static DB_TAB *HEAD = 0;
static DB_TAB *TAIL = 0;

/* global drawing variables */
XFORM unity_transform;
XFORM *global_transform = &unity_transform;  /* global xform matrix */

STACK *stack;

void db_free_component(); 		/* recycle memory for component */
int getbits();
int db_def_arch_recurse();
int db_def_files_recurse(); 
void db_contains_body(); 
void db_def_print();

/********************************************************/

DB_TAB * db_edit_pop(level)
int level;
{

    /* simply run through the db and return a pointer */
    /* to the rep with the highest <being_edited> variable */
    /* that is less than <level>.  Print an error if we  */
    /* find a <being_edited> equal or higher than <level> */
    /* The editor logic should ensure that this never can */
    /* happen... */

    int debug=0;

    DB_TAB *sp;
    DB_TAB *sp_best;
    int maxlevel;

    if (debug) printf("db_edit_pop: called with level %d\n", level);

    if (HEAD != (DB_TAB *)0) {
	for (sp=HEAD; sp!=(DB_TAB *)0; sp=sp->next) {
	    if (sp->being_edited > 0) {
		if (debug) printf("db_edit_pop: %s is level %d\n",
			sp->name, sp->being_edited);
		if (sp->being_edited > maxlevel) {
		    maxlevel = sp->being_edited;
		    sp_best = sp;
		}
	    }
	}
    }
    if (maxlevel > level) {
	printf("fatal error in db_edit_pop()\n"); 
	printf("db_edit_pop(): returning pointer to %s %d %d\n", sp_best->name, maxlevel, level); 
	return(NULL);
    } else if (maxlevel > 0) {
	if (debug) printf("db_edit_pop(): returning pointer to %s\n", sp_best->name); 
        return (sp_best);	
    } else {
	if (debug) printf("db_edit_pop(): returning NULL\n"); 
	return (NULL);
    }
}


DB_TAB *db_lookup(char *name)           /* find name in db */
{
    DB_TAB *sp;
    int debug=0;

    if (debug) printf("calling db_lookup with %s: ", name);
    if (HEAD != (DB_TAB *)0) {
	for (sp=HEAD; sp!=(DB_TAB *)0; sp=sp->next) {
	    if (strcmp(sp->name, name)==0) {
		if (debug) printf("found!\n");
		return(sp);
	    }
	}
    }
    if (debug) printf("not found!\n");
    return(0);               	/* 0 ==> not found */
} 

DB_TAB *db_install(s)		/* install s in db */
char *s;
{
    DB_TAB *sp;

    /* this is written to ensure that a db_print is */
    /* not in reversed order...  New structures are */
    /* added at the end of the list using TAIL      */

    sp = (DB_TAB *) emalloc(sizeof(struct db_tab));
    sp->name = emalloc(strlen(s)+1); /* +1 for '\0' */
    strcpy(sp->name, s);

    sp->next   = (DB_TAB *) 0; 
    sp->prev   = (DB_TAB *) 0; 
    sp->dbhead = (struct db_deflist *) 0; 
    sp->dbtail = (struct db_deflist *) 0; 
    sp->deleted = (struct db_deflist *) 0; 

    sp->vp_xmin = -100.0; 
    sp->vp_ymin = -100.0; 
    sp->vp_xmax = 100.0; 
    sp->vp_ymax = 100.0; 

    sp->minx = -100.0; 
    sp->miny = -100.0; 
    sp->maxx = 100.0; 
    sp->maxy = 100.0; 

    /* FIXME: good place for an ENV variables to let */
    /* user set the default grid color and step sizes */

    sp->grid_xd = 10.0;
    sp->grid_yd = 10.0;
    sp->grid_xs = 1.0;
    sp->grid_ys = 1.0;
    sp->grid_xo = 0.0;
    sp->grid_yo = 0.0;
    sp->grid_color = 3;		

    sp->display_state=G_ON;
    sp->logical_level=1;
    sp->lock_angle=0.0;
    sp->modified = 0;
    sp->being_edited = 0;
    sp->flag = 0;

    if (HEAD ==(DB_TAB *) 0) {	/* first element */
	HEAD = TAIL = sp;
    } else {
	sp->prev = TAIL;
	if (TAIL != (DB_TAB *) 0) {
	    TAIL->next = sp;		/* current tail points to new */
	}
	TAIL = sp;			/* and tail becomes new */
    }

    return (sp);
}

int ask(lp, s) /* ask a y/n question */
LEXER *lp;
char    *s;
{
    TOKEN token;
    char word[128];
    char buf[128];
    int done=0;

    rl_saveprompt();

    sprintf(buf,"%s: (y/n)? ", s);
    rl_setprompt(buf);

    while(!done && (token=token_get(lp, word)) != EOF) {
	switch(token) {
	    case IDENT: 	/* identifier */
		if (strncasecmp(word, "Y", 1) == 0) {
    		    rl_restoreprompt();
		    return(1);
		} else if (strncasecmp(word, "N", 1) == 0) {
    		    rl_restoreprompt();
		    return(0);
		} else {
	    	     printf("please answer by typing \"yes\" or \"no\": ");
		} 
		break;
	    case EOL:
	    case EOC:
	    	break; 
	    default:		/* command */
		 printf("please answer by typing \"yes\" or \"no\": ");
		 break;
        }
    }
    return(-1);
}


void db_purge(lp, s)			/* remove all definitions for s */
LEXER *lp;
char *s;
{
    DB_TAB *dp;
    DB_TAB *sp;
    DB_DEFLIST *p;
    int debug=0;
    char buf[MAXFILENAME];
    char buf2[MAXFILENAME];
    int flag=0;

    if ((sp=db_lookup(s))) { 	/* in memory */

        snprintf(buf, MAXFILENAME, "delete %s from memory", s);
        if (strcmp(lp->name, "STDIN") !=0 || ask(lp, buf)) {

	    flag=1;

	    if (debug) printf("db_purge: unlinking %s from db\n", s);

	    if (currep == sp) {
		currep = NULL;
		need_redraw++;
	    }

	    if (HEAD==sp) {
		HEAD=sp->next;
	    }

	    if (TAIL==sp) {
		TAIL=sp->prev;
	    }

	    if (sp->prev != (DB_TAB *) 0) {
		sp->prev->next = sp->next;
	    }
	    if (sp->next != (DB_TAB *) 0) {
		sp->next->prev = sp->prev;
	    }
	}

	for (p=sp->dbhead; p!=(struct db_deflist *)0; p=p->next) {
	    db_free_component(p);
	}
    
	free(sp);
    } 

    /* FIXME: later on this will get extended to include a search */
    /* path  instead of just a hard-wired "cells" subdirectory */

    /* now delete copy on disk */

    snprintf(buf, MAXFILENAME, "./cells/%s.d", s);
    if ((access(buf,F_OK)) == 0)  {	/* exists? */
        if (debug) printf("inside exists\n");
        sprintf(buf2, "delete %s from disk", buf);
        if (debug) printf("lexer name = %s", lp->name);
        if (strcmp(lp->name, "STDIN") !=0 || ask(lp, buf2)) {
	    if (debug) printf("removing %s\n", buf);
	    flag+=2;
	    remove(buf);
	}
    }

    /* we wiped out both memory and disk versions of the cell */
    /* so we'd better scan the DB and wipe out all calls to it */

    if (flag==3) {
    	for (dp=HEAD; dp!=(DB_TAB *)0; dp=dp->next) {
	    for (p=dp->dbhead; p!=(struct db_deflist *)0; p=p->next) {
		if (p->type == INST && strcmp(p->u.i->name, s)==0) {
		     /* this one has to go! */
		     printf("unresolvable references to %s in %s\n",
		     	s, dp->name);
			/* p->u.i->name); */
		     	
		}
     	    }
	}
    }
}

DB_DEFLIST *db_copy_component(p) 		/* create a copy of a component */
DB_DEFLIST *p;
{
    struct db_deflist *dp;

    if (p==NULL) {
    	printf("db_copy_component: can't copy a null component\n");
	return(NULL);
    }

    dp = (struct db_deflist *) emalloc(sizeof(struct db_deflist));
    dp->next = NULL;
    dp->prev = NULL;
    dp->type = p->type;

    switch (p->type) {
    case ARC:  /* arc definition */
	dp->u.a = (struct db_arc *) emalloc(sizeof(struct db_arc));
	dp->u.a->layer=p->u.a->layer;
	dp->u.a->opts=opt_copy(p->u.a->opts);
	dp->u.a->x1=p->u.a->x1;
	dp->u.a->y1=p->u.a->y1;
	dp->u.a->x2=p->u.a->x2;
	dp->u.a->y2=p->u.a->y2;
	dp->u.a->x3=p->u.a->x3;
	dp->u.a->y3=p->u.a->y3;
	break;
    case CIRC:  /* circle definition */
	dp->u.c = (struct db_circ *) emalloc(sizeof(struct db_circ));
	dp->u.c->layer=p->u.c->layer;
	dp->u.c->opts=opt_copy(p->u.c->opts);
	dp->u.c->x1=p->u.c->x1;
	dp->u.c->y1=p->u.c->y1;
	dp->u.c->x2=p->u.c->x2;
	dp->u.c->y2=p->u.c->y2;
	break;
    case LINE:  /* line definition */
	dp->u.l = (struct db_line *) emalloc(sizeof(struct db_line));
	dp->u.l->layer=p->u.l->layer;
	dp->u.l->opts=opt_copy(p->u.l->opts);
	dp->u.l->coords=coord_copy(p->u.l->coords);
	break;
    case NOTE:  /* note definition */
	dp->u.n = (struct db_note *) emalloc(sizeof(struct db_note));
	dp->u.n->layer=p->u.n->layer;
	dp->u.n->opts=opt_copy(p->u.n->opts);
	dp->u.n->text=strsave(p->u.n->text);
	dp->u.n->x=p->u.n->x;
	dp->u.n->y=p->u.n->y;
    	break;
    case OVAL:  /* oval definition */
	dp->u.o = (struct db_oval *) emalloc(sizeof(struct db_oval));
	dp->u.o->layer=p->u.o->layer;
	dp->u.o->opts=opt_copy(p->u.o->opts);
	dp->u.o->x1=p->u.o->x1;		/* first foci */
	dp->u.o->y1=p->u.o->y1;
	dp->u.o->x2=p->u.o->x2;		/* second foci */
	dp->u.o->y2=p->u.o->y2;
	dp->u.o->x3=p->u.o->x3;		/* point on curve */
	dp->u.o->y3=p->u.o->y3;
	break;
    case POLY:  /* polygon definition */
	dp->u.p = (struct db_poly *) emalloc(sizeof(struct db_poly));
	dp->u.p->layer=p->u.p->layer;
	dp->u.p->opts=opt_copy(p->u.p->opts);
	dp->u.p->coords=coord_copy(p->u.p->coords);
	break;
    case RECT:  /* rectangle definition */
	dp->u.r = (struct db_rect *) emalloc(sizeof(struct db_rect));
	dp->u.r->layer=p->u.r->layer;
	dp->u.r->opts=opt_copy(p->u.r->opts);
	dp->u.r->x1=p->u.r->x1;
	dp->u.r->y1=p->u.r->y1;
	dp->u.r->x2=p->u.r->x2;
	dp->u.r->y2=p->u.r->y2;
    	break;
    case TEXT:  /* text definition */
	dp->u.t = (struct db_text *) emalloc(sizeof(struct db_text));
	dp->u.t->layer=p->u.t->layer;
	dp->u.t->opts=opt_copy(p->u.t->opts);
	dp->u.t->text=strsave(p->u.t->text);
	dp->u.t->x=p->u.t->x;
	dp->u.t->y=p->u.t->y;
    	break;
    case INST:  /* instance call */
	dp->u.i = (struct db_inst *) emalloc(sizeof(struct db_inst));
	dp->u.i->opts=opt_copy(p->u.i->opts);
	dp->u.i->name=strsave(p->u.i->name);
	dp->u.i->x=p->u.i->x;
	dp->u.i->y=p->u.i->y;
	break;
    default:
    	printf("bad case in db_copy_component\n"); 
	break;
    }
    return (dp);
}

void db_free_component(p) 		/* recycle memory for component */
DB_DEFLIST *p;
{
    int debug=0;
    COORDS *coords;
    COORDS *ncoords;

    if (p == NULL) {
    	printf("db_free_component: can't free a NULL component\n");
    } else {
	switch (p->type) {

	case ARC:  /* arc definition */
	    if (debug) printf("db_purge: freeing arc\n");
	    free(p->u.a->opts);
	    free(p->u.a);
	    free(p);
	    break;

	case CIRC:  /* circle definition */
	    if (debug) printf("db_purge: freeing circle\n");
	    free(p->u.c->opts);
	    free(p->u.c);
	    free(p);
	    break;

	case LINE:  /* line definition */

	    if (debug) printf("db_purge: freeing line\n");
	    coords = p->u.l->coords;
	    while(coords != NULL) {
		ncoords = coords->next;
		free(coords);	
		coords=ncoords;
	    }
	    free(p->u.l->opts);
	    free(p->u.l);
	    free(p);
	    break;

	case NOTE:  /* note definition */

	    if (debug) printf("db_purge: freeing note\n");
	    free(p->u.n->opts);
	    free(p->u.n->text);
	    free(p->u.n);
	    free(p);
	    break;

	case OVAL:  /* oval definition */

	    if (debug) printf("db_purge: freeing oval\n");
	    free(p->u.o->opts);
	    free(p->u.o);
	    free(p);
	    break;

	case POLY:  /* polygon definition */

	    if (debug) printf("db_purge: freeing poly\n");
	    coords = p->u.p->coords;
	    while(coords != NULL) {
		ncoords = coords->next;
		free(coords);
		coords=ncoords;
	    }
	    free(p->u.p->opts);
	    free(p->u.p);
	    free(p);
	    break;

	case RECT:  /* rectangle definition */

	    if (debug) printf("db_purge: freeing rect\n");
	    free(p->u.r->opts);
	    free(p->u.r);
	    free(p);
	    break;

	case TEXT:  /* text definition */

	    if (debug) printf("db_purge: freeing text\n");
	    free(p->u.t->opts);
	    free(p->u.t->text);
	    free(p->u.t);
	    free(p);
	    break;

	case INST:  /* instance call */

	    if (debug) printf("db_purge: freeing instance call: %s\n",
		p->u.i->name);
	    free(p->u.i->name);
	    free(p->u.i->opts);
	    free(p->u.i);
	    free(p);
	    break;

	default:
	    eprintf("unknown record type (%d) in db_free_component\n", p->type );
	    break;
	}
    }
}

/* save a non-recursive archive of single cell to a file called "cell.d" */

int db_save(lp, sp, name)           	/* print db */
LEXER *lp;
DB_TAB *sp;
char *name;
{

    FILE *fp;
    char buf[MAXFILENAME];
    char buf2[MAXFILENAME];

    int debug=0;

    int err=0;

    mkdir("./cells", 0777);
    snprintf(buf, MAXFILENAME, "./cells/%s.d", name);

    /* check to see if would overwrite ask permission before doing so */
    /* but don't ask if device name matches save name */

    if (debug) {
	printf("in db_save, lp=%s\n", lp->name); 
	printf("device name=%s, save name=%s\n", sp->name, name);
	printf("access =%d\n", access(buf, F_OK));
    }

    if ((strcmp(sp->name, name) != 0) &&
       		(strcmp(lp->name, "STDIN") == 0) &&
       		(access(buf, F_OK) == 0)) {
        snprintf(buf2, MAXFILENAME, "overwrite %s on disk", name);
        if (ask(lp, buf2) == 0) {
	    printf("aborting save.\n");
	    return(1);
	}
    } 
    
    err+=((fp = fopen(buf, "w+")) == 0); 
    db_def_print(fp, sp, CELL); 
    err+=(fclose(fp) != 0);
    return(err);
}


int db_def_archive(sp) 
DB_TAB *sp;
{
    FILE *fp;
    char buf[MAXFILENAME];
    int err=0;
    DB_TAB *dp;
    char *s;

    mkdir("./cells", 0777);
    snprintf(buf, MAXFILENAME, "./cells/%s_I", sp->name);
    err+=((fp = fopen(buf, "w+")) == 0); 

    fp = fopen(buf, "w+"); 

    
    for (dp=HEAD; dp!=(DB_TAB *)0; dp=dp->next) {
	dp->flag=0;
    }
    fprintf(fp,"$FILES\n%s\n",sp->name);

    stack=NULL;
    db_def_files_recurse(fp,sp);
    while ((s=stack_pop(&stack))!=NULL) {
	fprintf(fp, "%s\n",s);
    }
    fprintf(fp,";\n\n");

    /* clear out all flag bits in cell definition headers */
    for (dp=HEAD; dp!=(DB_TAB *)0; dp=dp->next) {
	dp->flag=0;
    }

    /* now print out definitions of every cell from bottom to top */

    db_def_arch_recurse(fp,sp);
    db_def_print(fp, sp, ARCHIVE);

    err+=(fclose(fp) != 0);

    return(err);
}

/* When we retrieve an archive, it is important to first purge
   all the files that will be redefined by the archived.  If we've done
   something like: "EDIT FOO; ARC FOO; RET FOO;", then the ARCHIVE
   process will need to purge all the subcells of FOO in hierarchical
   order from top to bottom to avoid creating unreferenced stubs in the
   database at any moment.  It then needs to redefine all the cells from
   bottom to top to avoid using any cell before it has been defined. 
   The db_def_arch_recurse() function computes the bottom-to-top
   ordering by walking the definition tree and using a flag.  It prints
   names starting at the leaves of the tree and the flag bit prevents
   double definitions.  To do the initial purge properly, each archive
   file contains a "$FILE <top_cell> ...  <bottom_cell>" preamble which
   is generated by the following db_def_files_recurse() function.  This
   is a duplicate of the db_def_arch_recurse() function except that it
   uses the stack_push() and stack_pop() routines to reverse the order
   of the cell references. 
*/
 
 

int db_def_files_recurse(fp,sp) 
FILE *fp;
DB_TAB *sp;
{
    DB_DEFLIST *p; 

    for (p=sp->dbhead; p!=(struct db_deflist *)0; p=p->next) {
	if (p->type == INST && !(db_lookup(p->u.i->name)->flag)) {
	    db_def_files_recurse(fp, db_lookup(p->u.i->name)); 	/* recurse */
	    ((db_lookup(p->u.i->name))->flag)++;
	    stack_push(&stack, p->u.i->name);
	}
    }
    return(0); 	
}

int db_def_arch_recurse(fp,sp) 
FILE *fp;
DB_TAB *sp;
{
    DB_DEFLIST *p; 

    for (p=sp->dbhead; p!=(struct db_deflist *)0; p=p->next) {
	if (p->type == INST && !(db_lookup(p->u.i->name)->flag)) {
	    db_def_arch_recurse(fp, db_lookup(p->u.i->name)); 	/* recurse */
	    ((db_lookup(p->u.i->name))->flag)++;
	    db_def_print(fp, db_lookup(p->u.i->name), ARCHIVE);
	}
    }
    return(0); 	
}


int db_list_unsaved()
{
    DB_TAB *dp;
    int numunsaved=0;
    for (dp=HEAD; dp!=(DB_TAB *)0; dp=dp->next) {
    	if (dp->modified) {
	    printf("    device <%s> is modified!\n", dp->name);
	    numunsaved++;
	}
    }
    return(numunsaved);
}

int db_contains(name1, name2) /* return 0 if sp contains no reference to "name" */
char *name1;
char *name2;
{

    DB_TAB *sp;
    DB_TAB *dp;
    int retval = 0;
    int debug=0;
	 
    if ((sp=db_lookup(name1)) == NULL) {
    	printf("error in db_contains\n");
    }

    /* clear out all flag bits in cell definition headers */
    for (dp=HEAD; dp!=(DB_TAB *)0; dp=dp->next) {
	dp->flag=0;
    }

    if (debug) printf("db_contains called with %s %s\n", name1, name2);

    db_contains_body(sp, name2, &retval); /* recurse */
    return (retval);
}

void db_contains_body(sp,name,retval) 
DB_TAB *sp;
char *name;
int *retval;
{
    DB_DEFLIST *p; 
    int debug=0;

    /* FIXME protect against db_lookup returning NULL */
    /* which can happen if something is purged during an edit */
    /* perhaps it would also be good to have PURGE delete all references */
    /* to any PURGED cell? Also need to think about the policy for */
    /* what happens when a cell is edited that references non-existant */
    /* instances... */

    for (p=sp->dbhead; p!=(struct db_deflist *)0; p=p->next) {
	if (p->type == INST && 
	    db_lookup(p->u.i->name) != NULL &&
	    !(db_lookup(p->u.i->name)->flag)) {

		db_contains_body(db_lookup(p->u.i->name), name, retval);
		((db_lookup(p->u.i->name))->flag)++;

		if (debug) printf("%s %s %s\n", sp->name, p->u.i->name, name); 

		if (strcmp(p->u.i->name, name) == 0) {
		    (*retval)++;
		}
	}
    }
}


void db_def_print(fp, dp, mode) 
FILE *fp;
DB_TAB *dp;
int mode;
{
    COORDS *coords;
    DB_DEFLIST *p; 
    int i;

    if (mode == ARCHIVE) {
	/* fprintf(fp, "PURGE %s;\n", dp->name);  */
	fprintf(fp, "EDIT %s;\n", dp->name);
	fprintf(fp, "SHOW #E;\n");
	fprintf(fp, "DISP OFF;\n"); 
    }

    fprintf(fp, "LOCK %g;\n", dp->lock_angle);
    fprintf(fp, "LEVEL %d;\n", dp->logical_level);
    fprintf(fp, "GRID :C%d %g %g %g %g %g %g;\n", 
        dp->grid_color,
    	dp->grid_xd, dp->grid_yd,
	dp->grid_xs, dp->grid_ys,
	dp->grid_xo, dp->grid_yo);

    fprintf(fp, "WIN %g,%g %g,%g;\n",
    	dp->minx, dp->miny, dp->maxx, dp->maxy);
    	/* dp->vp_xmin, dp->vp_ymin, dp->vp_xmax, dp->vp_ymax); */

    for (p=dp->dbhead; p!=(struct db_deflist *)0; p=p->next) {
	switch (p->type) {
        case ARC:  /* arc definition */

	    fprintf(fp, "ADD A%d ", p->u.a->layer);
	    db_print_opts(fp, p->u.a->opts, ARC_OPTS);

	    fprintf(fp, "%g,%g %g,%g %g,%g;\n",
		p->u.a->x1,
		p->u.a->y1,
		p->u.a->x2,
		p->u.a->y2,
		p->u.a->x3,
		p->u.a->y3
	    );

	    break;

        case CIRC:  /* circle definition */

	    fprintf(fp, "ADD C%d ", p->u.c->layer);
	    db_print_opts(fp, p->u.c->opts, CIRC_OPTS);

	    fprintf(fp, "%g,%g %g,%g;\n",
		p->u.c->x1,
		p->u.c->y1,
		p->u.c->x2,
		p->u.c->y2
	    );
	    break;

        case LINE:  /* line definition */
	    fprintf(fp, "ADD L%d ", p->u.l->layer);
	    db_print_opts(fp, p->u.l->opts, LINE_OPTS);

	    i=1;
	    coords = p->u.l->coords;
	    while(coords != NULL) {
		fprintf(fp, " %g,%g", coords->coord.x, coords->coord.y);
		if ((coords = coords->next) != NULL && (++i)%7 == 0) {
		    fprintf(fp,"\n");
		}
	    }
	    fprintf(fp, ";\n");

	    break;

        case NOTE:  /* note definition */

	    fprintf(fp, "ADD N%d ", p->u.n->layer);
	    db_print_opts(fp, p->u.n->opts, NOTE_OPTS);

	    fprintf(fp, "\"%s\" %g,%g;\n",
		p->u.n->text,
		p->u.n->x,
		p->u.n->y
	    );
	    break;

        case OVAL:  /* oval definition */

	    fprintf(fp, "#add OVAL not implemented yet\n");
	    break;

        case POLY:  /* polygon definition */

	    fprintf(fp, "ADD P%d", p->u.p->layer);
	    db_print_opts(fp, p->u.p->opts, POLY_OPTS);

	    i=1;
	    coords = p->u.p->coords;
	    while(coords != NULL) {
		fprintf(fp, " %g,%g", coords->coord.x, coords->coord.y);
		if ((coords = coords->next) != NULL && (++i)%7 == 0) {
		    fprintf(fp,"\n");
		}
	    }
	    fprintf(fp, ";\n");

	    break;

	case RECT:  /* rectangle definition */

	    fprintf(fp, "ADD R%d ", p->u.r->layer);
	    db_print_opts(fp, p->u.r->opts, RECT_OPTS);

	    fprintf(fp, "%g,%g %g,%g;\n",
		p->u.r->x1,
		p->u.r->y1,
		p->u.r->x2,
		p->u.r->y2
	    );
	    break;

        case TEXT:  /* text definition */

	    fprintf(fp, "ADD T%d ", p->u.t->layer); 
	    db_print_opts(fp, p->u.t->opts, TEXT_OPTS);

	    fprintf(fp, "\"%s\" %g,%g;\n",
		p->u.t->text,
		p->u.t->x,
		p->u.t->y
	    );
	    break;

        case INST:  /* instance call */
	    fprintf(fp, "ADD %s ", p->u.i->name);
	    db_print_opts(fp, p->u.i->opts, INST_OPTS);
	    fprintf(fp, "%g,%g;\n",
		p->u.i->x,
		p->u.i->y
	    );
	    break;

	default:
	    eprintf("unknown record type (%d) in db_def_print\n", p->type );
	    break;
	}

    }

    if (mode == ARCHIVE) {
	fprintf(fp, "DISP ON;\n"); 
	fprintf(fp, "SAVE;\n");
	fprintf(fp, "EXIT;\n\n");
    }
}

void db_unlink_component(cell, dp) 
DB_TAB *cell;
struct db_deflist *dp;
{

    if(dp == NULL) {
    	printf("can't delete a null instance\n");
    }else if(dp->prev == NULL ) {		/* first one the list */
	cell->dbhead = dp->next;
	if (dp->next != NULL) {
	    dp->next->prev = dp->prev;
	}
    } else if(dp->next == NULL ) {	/* last in the list */
	dp->prev->next = dp->next;
	cell->dbtail = dp->prev;
    } else {				/* somewhere in the chain */
	dp->prev->next = dp->next;
	dp->next->prev = dp->prev;
    }

    if (currep->deleted != NULL) {
	db_free_component(currep->deleted);	/* gone for good now! */
    } 
    currep->deleted = dp;		/* save for one level of undo */
    currep->modified++;
}

void db_insert_component(cell,dp) 
DB_TAB *cell;
struct db_deflist *dp;
{
    /* this may be a recycled cell pointer */
    /* from a delete, so clean it up */

    dp->prev = NULL;
    dp->next = NULL;

    /* add definition at *end* of list */
    if (cell->dbhead == NULL) {
	cell->dbhead = cell->dbtail = dp;
    } else {
	dp->prev = cell->dbtail; 	/* doubly linked list */
	if (cell->dbtail != NULL) {
	    cell->dbtail->next = dp;
	}
	cell->dbtail = dp;
    }

    cell->modified++;
}

void db_set_layer(p, new_layer) /* set the layer of any component */
struct db_deflist *p;
int new_layer;
{
    if (p == NULL) {
    	printf("db_set_layer: can't change layer on null component!\n");
    } else {
	switch (p->type) {
	    case ARC:
		if (new_layer) p->u.a->layer = new_layer;
		break;
	    case CIRC:
		if (new_layer) p->u.c->layer = new_layer;
		break;
	    case INST:
		break;
	    case LINE:
		if (new_layer) p->u.l->layer = new_layer;
		break;
	    case NOTE:
		if (new_layer) p->u.n->layer = new_layer;
		break;
	    case OVAL:
		if (new_layer) p->u.o->layer = new_layer;
		break;
	    case POLY:
		if (new_layer) p->u.p->layer = new_layer;
		break;
	    case RECT:
		if (new_layer) p->u.r->layer = new_layer;
		break;
	    case TEXT:
		if (new_layer) p->u.t->layer = new_layer;
		break;
	    default:
		printf("db_change_layer: unknown case\n");
		break;
	}
    }
}

void db_move_component(p, dx, dy) /* move any component by dx, dy */
struct db_deflist *p;
double dx, dy;
{
    COORDS *coords;

    switch (p->type) {
    case ARC:  /* arc definition */
	p->u.a->x1 += dx;
	p->u.a->y1 += dy;
	p->u.a->x2 += dx;
	p->u.a->y2 += dy;
	p->u.a->x3 += dx;
	p->u.a->y3 += dy;
	break;
    case CIRC:  /* circle definition */
	p->u.c->x1 += dx;
	p->u.c->y1 += dy;
	p->u.c->x2 += dx;
	p->u.c->y2 += dy;
	break;
    case LINE:  /* line definition */
	coords = p->u.l->coords;
	while(coords != NULL) {
	    coords->coord.x += dx;
	    coords->coord.y += dy;
	    coords = coords->next;
	}
	break;
    case NOTE:  /* note definition */
	p->u.n->x += dx;
	p->u.n->y += dy;
	break;
    case OVAL:  /* oval definition */
        /* FIXME: Not implemented */
	break;
    case POLY:  /* polygon definition */
	coords = p->u.p->coords;
	while(coords != NULL) {
	    coords->coord.x += dx;
	    coords->coord.y += dy;
	    coords = coords->next;
	}
	break;
    case RECT:  /* rectangle definition */
	p->u.r->x1 += dx;
	p->u.r->y1 += dy;
	p->u.r->x2 += dx;
	p->u.r->y2 += dy;
	break;
    case TEXT:  /* text definition */
	p->u.t->x += dx;
	p->u.t->y += dy;
	break;
    case INST:  /* instance call */
	p->u.i->x += dx;
	p->u.i->y += dy;
	break;

    default:
	eprintf("unknown record type (%d) in db_move_component\n", p->type );
	break;
    }
}

int db_add_arc(cell, layer, opts, x1,y1,x2,y2,x3,y3) 
DB_TAB *cell;
int layer;
OPTS *opts;
NUM x1,y1,x2,y2,x3,y3;
{
    struct db_arc *ap;
    struct db_deflist *dp;

    ap = (struct db_arc *) emalloc(sizeof(struct db_arc));
    dp = (struct db_deflist *) emalloc(sizeof(struct db_deflist));
    dp->next = NULL;
    dp->prev = NULL;

    dp->u.a = ap;
    dp->type = ARC;

    ap->layer=layer;
    ap->opts=opts;
    ap->x1=x1;
    ap->y1=y1;
    ap->x2=x2;
    ap->y2=y2;
    ap->x3=x3;
    ap->y3=y3;

    db_insert_component(cell,dp);
    return(0);
}

int db_add_circ(cell, layer, opts, x1,y1,x2,y2)
DB_TAB *cell;
int layer;
OPTS *opts;
NUM x1,y1,x2,y2;
{
    struct db_circ *cp;
    struct db_deflist *dp;
 
    cp = (struct db_circ *) emalloc(sizeof(struct db_circ));
    dp = (struct db_deflist *) emalloc(sizeof(struct db_deflist));
    dp->next = NULL;
    dp->prev = NULL;

    dp->u.c = cp;
    dp->type = CIRC;

    cp->layer=layer;
    cp->opts=opts;
    cp->x1=x1;
    cp->y1=y1;
    cp->x2=x2;
    cp->y2=y2;

    db_insert_component(cell,dp);
    return(0);
}

int db_add_line(cell, layer, opts, coords)
DB_TAB *cell;
int layer;
OPTS *opts;
COORDS *coords;
{
    struct db_line *lp;
    struct db_deflist *dp;

    lp = (struct db_line *) emalloc(sizeof(struct db_line));
    dp = (struct db_deflist *) emalloc(sizeof(struct db_deflist));
    dp->next = NULL;
    dp->prev = NULL;
    dp->u.l = lp;
    dp->type = LINE;
    lp->layer=layer;
    lp->opts=opts;
    lp->coords=coords;

    db_insert_component(cell,dp);
    return(0);
}

int db_add_oval(cell, layer, opts, x1,y1,x2,y2,x3,y3) 
DB_TAB *cell;
int layer;
OPTS *opts;
NUM x1,y1, x2,y2, x3,y3;
{
    struct db_oval *op;
    struct db_deflist *dp;

    op = (struct db_oval *) emalloc(sizeof(struct db_oval));
    dp = (struct db_deflist *) emalloc(sizeof(struct db_deflist));
    dp->next = NULL;
    dp->prev = NULL;

    dp->u.o = op;
    dp->type = OVAL;

    op->layer=layer;
    op->opts=opts;
    op->x1=x1;		/* first foci */
    op->y1=y1;
    op->x2=x2;		/* second foci */
    op->y2=y2;
    op->x3=x3;		/* point on curve */
    op->y3=y3;

    db_insert_component(cell,dp);
    return(0);
}

int db_add_poly(cell, layer, opts, coords)
DB_TAB *cell;
int layer;
OPTS *opts;
COORDS *coords;
{
    struct db_poly *pp;
    struct db_deflist *dp;
    int debug = 0;
 
    pp = (struct db_poly *) emalloc(sizeof(struct db_poly));
    dp = (struct db_deflist *) emalloc(sizeof(struct db_deflist));
    dp->next = NULL;
    dp->prev = NULL;

    dp->u.p = pp;
    dp->type = POLY;

    if (debug) printf("db_add_poly: setting poly layer %d\n", layer);
    pp->layer=layer;
    pp->opts=opts;
    pp->coords=coords;

    db_insert_component(cell,dp);
    return(0);
}

int db_add_rect(cell, layer, opts, x1,y1,x2,y2)
DB_TAB *cell;
int layer;
OPTS *opts;
NUM x1,y1,x2,y2;
{
    struct db_rect *rp;
    struct db_deflist *dp;

    rp = (struct db_rect *) emalloc(sizeof(struct db_rect));
    dp = (struct db_deflist *) emalloc(sizeof(struct db_deflist));
    dp->next = NULL;
    dp->prev = NULL;

    dp->u.r = rp;
    dp->type = RECT;

    rp->layer=layer;
    rp->x1=x1;
    rp->y1=y1;
    rp->x2=x2;
    rp->y2=y2;
    rp->opts = opts;

    db_insert_component(cell,dp);
    return(0);
}

int db_add_note(cell, layer, opts, string ,x,y) 
DB_TAB *cell;
int layer;
OPTS *opts;
char *string;
NUM x,y;
{
    struct db_note *np;
    struct db_deflist *dp;
 
    np = (struct db_note *) emalloc(sizeof(struct db_note));
    dp = (struct db_deflist *) emalloc(sizeof(struct db_deflist));
    dp->next = NULL;
    dp->prev = NULL;

    dp->u.n = np;
    dp->type = NOTE;

    np->layer=layer;
    np->text=string;
    np->opts=opts;
    np->x=x;
    np->y=y;

    db_insert_component(cell,dp);
    return(0);
}

int db_add_text(cell, layer, opts, string ,x,y) 
DB_TAB *cell;
int layer;
OPTS *opts;
char *string;
NUM x,y;
{
    struct db_text *tp;
    struct db_deflist *dp;
 
    tp = (struct db_text *) emalloc(sizeof(struct db_text));
    dp = (struct db_deflist *) emalloc(sizeof(struct db_deflist));
    dp->next = NULL;
    dp->prev = NULL;

    dp->u.t = tp;
    dp->type = TEXT;

    tp->layer=layer;
    tp->text=string;
    tp->opts=opts;
    tp->x=x;
    tp->y=y;

    db_insert_component(cell,dp);
    return(0);
}

int db_add_inst(cell, subcell, opts, x, y)
DB_TAB *cell;
DB_TAB *subcell;
OPTS *opts;
NUM x,y;
{
    struct db_inst *ip;
    struct db_deflist *dp;
 
    ip = (struct db_inst *) emalloc(sizeof(struct db_inst));
    dp = (struct db_deflist *) emalloc(sizeof(struct db_deflist));
    dp->next = NULL;
    dp->prev = NULL;

    dp->u.i = ip;
    dp->type = INST;

    /* ip->def=subcell; */
    ip->name=strsave(subcell->name);

    ip->x=x;
    ip->y=y;
    ip->opts=opts;
    db_insert_component(cell,dp);
    return(0);
}

/*
main () { 	

    DB_TAB *currep, *oldrep;

    currep = db_install("1foo");
    currep = db_install("2xyz");
	db_add_rect(currep,1,2.0,3.0,4.0,5.0);
	db_add_rect(currep,6,7.0,8.0,9.0,10.0);
    oldrep = currep;
    currep = db_install("3abc");
	db_add_rect(currep,11,12.0,13.0,14.0,15.0);
	db_add_inst(currep,oldrep,16.0,17.0);
    db_print();
}
*/

OPTS *opt_copy(OPTS *opts)
{
    OPTS *tmp;
    tmp = (OPTS *) emalloc(sizeof(OPTS));
    
    /* set defaults */
    tmp->font_size = opts->font_size;
    tmp->font_num = opts->font_num;
    tmp->mirror = opts->mirror;
    tmp->rotation = opts->rotation;
    tmp->width = opts->width;
    tmp->aspect_ratio = opts->aspect_ratio;
    tmp->scale = opts->scale;
    tmp->slant = opts->slant;
    tmp->cname = opts->cname;
    tmp->sname = opts->sname;

    return(tmp);
}     

void opt_set_defaults(OPTS *opts)
{
    opts->font_size = 10.0;       /* :F<font_size> */
    opts->mirror = MIRROR_OFF;    /* :M<x,xy,y>    */
    opts->font_num=0;		  /* :N<font_num> */
    opts->rotation = 0.0;         /* :R<rotation,resolution> */
    opts->width = 0.0;            /* :W<width> */
    opts->aspect_ratio = 1.0;     /* :Y<yx_aspect_ratio> */
    opts->scale=1.0;              /* :X<scale> */
    opts->slant=0.0;              /* :Z<slant_degrees> */
    opts->cname= (char *) NULL;   /* component name */
    opts->sname= (char *) NULL;   /* signal name */
}

OPTS *opt_create()
{
    OPTS *tmp;
    tmp = (OPTS *) emalloc(sizeof(OPTS));
    
    opt_set_defaults(tmp);

    return(tmp);
}     

void db_print_opts(fp, popt, validopts) /* print options */
FILE *fp;
OPTS *popt;
/* validopts is a string which sets which options are printed out */
/* an example for a line is "W", text uses "MNRYZF" */
char *validopts;
{
    char *p;

    if (popt->cname != NULL) {
    	fprintf(fp, "%s ", popt->cname);
    }

    if (popt->sname != NULL) {
    	fprintf(fp, "%s ", popt->sname);
    }

    for (p=validopts; *p != '\0'; p++) {
    	switch(toupper(*p)) {
	    case 'F':
		fprintf(fp, ":F%g ", popt->font_size);
	    	break;
	    case 'M':
		switch(popt->mirror) {
		    case MIRROR_OFF:
		    	break;
		    case MIRROR_X:
	    		fprintf(fp, ":MX ");
			break;
		    case MIRROR_Y:
	    		fprintf(fp, ":MY ");
			break;
		    case MIRROR_XY:
	    		fprintf(fp, ":MXY ");
			break;
		    default:
	    		weprintf("invalid mirror case\n");
		    	break;	
		}
	    	break;
	    case 'N':
		if (popt->font_num != 0 && popt->font_num != 1) {
		    fprintf(fp, ":N%d ", popt->font_num);
		}
	    	break;
	    case 'R':
		if (popt->rotation != 0.0) {
		    fprintf(fp, ":R%g ", popt->rotation);
		}
	    	break;
	    case 'W':
		if (popt->width != 0.0) {
		    fprintf(fp, ":W%g ", popt->width);
		}
	    	break;
	    case 'X':
		if (popt->scale != 1.0) {
		    fprintf(fp, ":X%g ", popt->scale);
		}
	    	break;
	    case 'Y':
		if (popt->aspect_ratio != 1.0) {
		    fprintf(fp, ":Y%g ", popt->aspect_ratio);
		}
	    	break;
	    case 'Z':
		if (popt->slant != 0.0) {
		    fprintf(fp, ":Z%g ", popt->slant);
		}
	    	break;
	    default:
		weprintf("invalid option %c\n", *p); 
	    	break;
	}
    }
}


char *strsave(s)   /* save string s somewhere */
char *s;
{
    char *p;

    if ((p = (char *) emalloc(strlen(s)+1)) != NULL)
	strcpy(p,s);
    return(p);
}

/******************** plot geometries *********************/

/* ADD Amask [.cname] [@sname] [:Wwidth] [:Rres] coord coord coord */ 
void do_arc(def, bb, mode)
DB_DEFLIST *def;
BOUNDS *bb;
int mode;
{
    NUM x1,y1,x2,y2,x3,y3;
    int res;
    double seg;
    double nseg;
    double dtheta3, theta1, dtheta2, theta;
    double q, r, x0, y0;
    int debug=0;

    COORDS *cp;
    DB_DEFLIST *dp;
    struct db_line *lp;

    x1=def->u.a->x1;	/* #1 end point */
    y1=def->u.a->y1;

    x2=def->u.a->x2;	/* #2 end point */
    y2=def->u.a->y2;

    x3=def->u.a->x3;	/* point on curve */
    y3=def->u.a->y3;

    jump(bb,mode); set_layer(def->u.a->layer, ARC);

    if ((def->u.a->opts->rotation) != 0.0) {
	res = (int) (360.0/def->u.a->opts->rotation);	/* resolution */
    } else {
    	res = 64;
    }

    if( def->u.a->opts->width != 0.0) {
        /* render with width using the line drawing routine */
	dp = (struct db_deflist *) emalloc(sizeof(struct db_deflist));
	dp->next = NULL;
	dp->prev = NULL;
	dp->type = LINE;
        lp = (struct db_line *) emalloc(sizeof(struct db_line));
	dp->u.l = lp;
	dp->u.l->layer=def->u.a->layer;
	dp->u.l->opts=opt_create();
        dp->u.l->opts->width = def->u.a->opts->width;
    }

    /* check to see if (x1,y1), (x2,y2), (x3,y3) are colinear,
       because if they are then our arc algorithm blows up, 
       use a simple line segment instead.

    	does (x1,y1) + alpha*(x2-x1, y2-y1) = (x3,y3) ?

	alphax*(x2-x1) + x1 = x3
	alphax = (x3-x1)/(x2-x1)
	alphay = (y3-y1)/(y2-y1)

	colinear iff:

	(x3-x1)/(x2-x1) = (y3-y1)/(y2-y1)

	rearrange to avoid divbyzero:

	collinear iff ((x3-x1)*(y2-y1) - (y3-y1)*(x2-x1) == 0)

	note: it should be OK to test for exact equality here because
	this test uses only subtraction and multiplication, and
	the points have already been snapped to integer multiples
	of the resolution setting.
    */

    if (((x3-x1)*(y2-y1) - (y3-y1)*(x2-x1) == 0)) { 	/* colinear ? */

	/* draw straight line from x1,y1 to x2,y2 */

 	if( def->u.a->opts->width == 0.0) {
	    draw(x1, y1, bb, mode);
	    draw(x2, y2, bb, mode);
	} else {
	    cp = coord_new(x1, y1);
	    dp->u.l->coords=cp;
	    coord_append(cp, x2, y2);
	}

    } else {	/* otherwise compute arc */

	/*
	Given two points:  p1:x1,y1 p2:x2,y2 at the ends of a circular arc and
	a third point p3:x3,y3 on the arc itself, then find the center of
	the circle and the radius.

	Note that a perpendicular from the center of the line segment
	drawn between p1 and p2, will intersect the perpendicular extended
	from the center of the line segment p2p3 at the center of the circle.

	Center points:
	    { (x1+x2)/2 , (y1+y2)/2 }
	    { (x2+x3)/2 , (y3+y3)/2 }

	Perpendiculars are constructed by computing dx and dy and then taking
	these as the vector { -dy, dx }.

	So the equation for the center of the circle is found by finding P,Q
	such that:
	    { (x1+x2)/2 , (y1+y2)/2 } + P * { y1-y2, x2-x1 } =
	    { (x2+x3)/2 , (y2+y3)/2 } + Q * { y2-y3, x3-x2 }
	*/

	q = ((y1-y2)*((y2+y3)/2 - (y1+y2)/2) - (x2-x1)*((x2+x3)/2 - (x1+x2)/2));
	q /= ((y2-y3)*(x2-x1) - (x3-x2)*(y1-y2));

	x0 = (x2+x3)/2 + q*(y2-y3);
	y0 = (y2+y3)/2 + q*(x3-x2);   

	r = sqrt(pow((x3-x0),2.0)  +pow((y3-y0),2.0));

	theta1 = atan2(y1-y0, x1-x0);
	dtheta2 = atan2(y2-y0, x2-x0) - theta1;
	dtheta3 = atan2(y3-y0, x3-x0) - theta1;

	if (dtheta2 < 0.0) {
	    dtheta2 += 2.0*M_PI;
	}
	if (dtheta3 < 0.0) {
	    dtheta3 += 2.0*M_PI;
	}

	if ((dtheta3 > dtheta2)) {
	    nseg = res-fabs(floor(res*(dtheta2)/(2.0*M_PI)));
	    if (debug) printf("YY t1=%g, dt2=%g, dt3=%g, nseg=%g\n", 
		    theta1, dtheta2, dtheta3, nseg);
	} else {
	    nseg = fabs(floor(res*(dtheta2)/(2.0*M_PI)));
	    if (debug) printf("XX t1=%g, dt2=%g, dt3=%g, nseg=%g\n", 
		    theta1, dtheta2, dtheta3, nseg);
	}
	if (debug) fflush(stdout);

	for (seg=0; seg<=nseg; seg++) {
	    if ((dtheta3 > dtheta2)) {
		theta=dtheta2+theta1+seg*(2.0*M_PI-dtheta2)/nseg; 
	    } else {
		theta=theta1+seg*(dtheta2)/nseg; 
	    }
	    if( def->u.a->opts->width == 0.0) {
		draw(x0+r*cos(theta), y0+r*sin(theta), bb, mode);
	    } else {
		if (seg==0) {
		    cp = coord_new(x0+r*cos(theta), y0+r*sin(theta));
		    dp->u.l->coords=cp;
		} else {
		    coord_append(cp, x0+r*cos(theta), y0+r*sin(theta));
		}
	    }
	}
    } 
    
    if( def->u.a->opts->width != 0.0) {
    	do_line(dp, bb, mode);		/* draw it */
	db_free_component(dp);		/* now destroy it */
    }
}

/* ADD Cmask [.cname] [@sname] [:Yyxratio] [:Wwidth] [:Rres] coord coord */
void do_circ(def, bb, mode)
DB_DEFLIST *def;
BOUNDS *bb;
int mode;		/* drawing mode */
{
    int i;
    double r1, r2,theta,x,y,x1,y1,x2,y2;
    double theta_start;
    int res;

    COORDS *cp;
    DB_DEFLIST *dp;
    struct db_line *lp;

    if ((def->u.c->opts->rotation) != 0.0) {
	res = (int) (360.0/def->u.c->opts->rotation);	/* resolution */
    } else {
    	res = 64;
    }

    /* printf("# rendering circle\n"); */

    x1 = (double) def->u.c->x1; 	/* center point */
    y1 = (double) def->u.c->y1;

    x2 = (double) def->u.c->x2;	/* point on circumference */
    y2 = (double) def->u.c->y2;

    if (mode==D_RUBBER) {
    	xwin_draw_circle(x1,y1);
    	xwin_draw_circle(x2,y2);
    }
    
    jump(bb,mode); set_layer(def->u.c->layer, CIRC);

    x = (double) (x2-x1);
    y = (double) (y2-y1);

    /* this next snippet of code works on the realization that an oval */
    /* is really just the vector sum of two counter-rotating phasors */
    /* of different lengths.  If r1=r and r2=0, you get a circle.  If you */
    /* solve the math for r1, r2 as a function of major/minor axis widths */
    /* you get the following.  If :Y<aspect_ratio> is unset, then the following */
    /* defaults to a perfectly round circle... */

    r1 = (1.0+def->u.c->opts->aspect_ratio)*sqrt(x*x+y*y)/2.0;
    r2 = (1.0-def->u.c->opts->aspect_ratio)*sqrt(x*x+y*y)/2.0;

    theta_start = atan2(x,y);
    if( def->u.c->opts->width != 0.0) {
        /* render a circle with width using the line drawing routine */
	dp = (struct db_deflist *) emalloc(sizeof(struct db_deflist));
	dp->next = NULL;
	dp->prev = NULL;
	dp->type = LINE;
        lp = (struct db_line *) emalloc(sizeof(struct db_line));
	dp->u.l = lp;
	dp->u.l->layer=def->u.c->layer;
	dp->u.l->opts=opt_create();
        dp->u.l->opts->width = def->u.c->opts->width;
    } else {
	startpoly(bb,mode);
    }

    for (i=0; i<=res; i++) {
	theta = ((double) i)*2.0*M_PI/((double) res);
	x = r1*sin(theta_start-theta) + r2*sin(theta_start+theta);
	y = r1*cos(theta_start-theta) + r2*cos(theta_start+theta);
	if( def->u.c->opts->width == 0.0) {
	    draw(x1+x, y1+y, bb, mode);
	} else {
	    if (i==0) {
	    	cp = coord_new(x1+x, y1+y);
		dp->u.l->coords=cp;
	    } else {
		coord_append(cp, x1+x, y1+y);
	    }
	}
    }
    if( def->u.c->opts->width != 0.0) {
    	do_line(dp, bb, mode);		/* draw it */
	db_free_component(dp);		/* now destroy it */
    } else {
        endpoly(bb,mode);	
    }
}


/* ADD Lmask [.cname] [@sname] [:Wwidth] coord coord [coord ...] */

void do_line(def, bb, mode)
DB_DEFLIST *def;
BOUNDS *bb;
int mode; 	/* drawing mode */
{
    COORDS *temp;

    double x1,y1,x2,y2,x3,y3,dx,dy,a;
    double xa,ya,xb,yb,xc,yc,xd,yd;
    double k,dxn,dyn;
    double width=0.0;
    int segment;
    int debug=0;	/* set to 1 for copious debug output */

    width = def->u.l->opts->width;

    /* there are four cases for rendering lines with miters 
     *
     * "===" is rendered segment, "|" is a square end,
     * "/" is a mitered end, "----" is a non-rendered segment.
     * 
     * 1) single segment: 
     *      (temp->next == NULL)
     * 
     *	    |=====|
     *
     * 2) first segment, but more than one
     *      (temp->next != NULL)
     *
     *	    |=====/-----/...
     *
     * 3) middle segment bracketed by active segments:
     *      (temp->next != NULL)
     *
     *      -----/======/-----
     *
     * 4) final segment, preceeded by an active segment:
     *      (temp->next == NULL)
     *      -----/======|
     */

    /* printf("# rendering line\n"); */
    set_layer(def->u.l->layer, LINE);

    temp = def->u.l->coords;
    x2=temp->coord.x;
    y2=temp->coord.y;
    temp = temp->next;

    if (mode==D_RUBBER) {
	xwin_draw_circle(x2,y2);
    }

    segment = 0;
    do {
	x1=x2; x2=temp->coord.x;
	y1=y2; y2=temp->coord.y;

	if (mode==D_RUBBER) {
	    xwin_draw_circle(x2,y2);
	}

	if (width != 0) {
	    if (segment == 0) {
		dx = x2-x1; 
		dy = y2-y1;
		a = 1.0/(2.0*sqrt(dx*dx+dy*dy));
		if (debug) printf("# in 1, a=%g\n",a);
		dx *= a; 
		dy *= a;
	    } else {
		if (debug) printf("# in 2\n"); 
		dx=dxn;
		dy=dyn;
	    }

	    if ( temp->next != NULL ) {
		x3=temp->next->coord.x;
		y3=temp->next->coord.y;
		dxn = x3-x2; 
		dyn = y3-y2;

		/* prevent a singularity if the last */
		/* rubber band segment has zero length */
		if (dxn==0 && dyn==0) {
		    a = 0;
		} else {
		    a = 1.0/(2.0*sqrt(dxn*dxn+dyn*dyn));
		}

		if (debug) printf("# in 3, a=%g\n",a); 
		dxn *= a; 
		dyn *= a;
	    }
	    
	    if ( temp->next == NULL && segment == 0) {

		if (debug) printf("# in 4\n"); 
		xa = x1+dy*width; ya = y1-dx*width;
		xb = x2+dy*width; yb = y2-dx*width;
		xc = x2-dy*width; yc = y2+dx*width;
		xd = x1-dy*width; yd = y1+dx*width;

	    } else if (temp->next != NULL && segment == 0) {  

		if (debug) printf("# in 5\n"); 
		xa = x1+dy*width; ya = y1-dx*width;
		xd = x1-dy*width; yd = y1+dx*width;

		if (fabs(dx+dxn) < EPS) {
		    k = (dx - dxn)/(dyn + dy); 
		    if (debug) printf("# in 6, k=%g\n",k);
		} else {
		    k = (dyn - dy)/(dx + dxn);
		    if (debug) printf("# in 7, k=%g\n",k);
		}

		/* check for co-linearity of segments */
		if ((fabs(dx-dxn)<EPS && fabs(dy-dyn)<EPS) ||
		    (fabs(dx+dxn)<EPS && fabs(dy+dyn)<EPS)) {
		    if (debug) printf("# in 8\n"); 
		    xb = x2+dy*width; yb = y2-dx*width;
		    xc = x2-dy*width; yc = y2+dx*width;
		} else { 
		    if (debug) printf("# in 19\n");
		    xb = x2+dy*width + k*dx*width;
		    yb = y2-dx*width + k*dy*width;
		    xc = x2-dy*width - k*dx*width; 
		    yc = y2+dx*width - k*dy*width;
		}

	    } else if ((temp->next == NULL && segment >= 0) ||
	              ( temp->next->coord.x == x2 && 
		        temp->next->coord.y == y2)) {

		if (debug) printf("# in 10\n");
		xb = x2+dy*width; yb = y2-dx*width;
		xc = x2-dy*width; yc = y2+dx*width;

	    } else if (temp->next != NULL && segment >= 0) {  

		if (debug) printf("# in 11\n");
		if (fabs(dx+dxn) < EPS) {
		    k = (dx - dxn)/(dyn + dy); 
		    if (debug) printf("# in 12, k=%g\n",k);
		} else {
		    if (debug) printf("# in 13, k=%g\n",k); 
		    k = (dyn - dy)/(dx + dxn);
		}

		/* check for co-linearity of segments */
		if ((fabs(dx-dxn)<EPS && fabs(dy-dyn)<EPS) ||
		    (fabs(dx+dxn)<EPS && fabs(dy+dyn)<EPS)) {
		    if (debug) printf("# in 8\n"); 
		    xb = x2+dy*width; yb = y2-dx*width;
		    xc = x2-dy*width; yc = y2+dx*width;
		} else { 
		    if (debug) printf("# in 9\n"); 
		    xb = x2+dy*width + k*dx*width;
		    yb = y2-dx*width + k*dy*width;
		    xc = x2-dy*width - k*dx*width; 
		    yc = y2+dx*width - k*dy*width;
		}
	    }

	    jump(bb,mode);
	
	    draw(xa, ya, bb, mode);
	    draw(xb, yb, bb, mode);

	    if( width != 0) {
		draw(xc, yc, bb, mode);
		draw(xd, yd, bb, mode);
		draw(xa, ya, bb, mode);
	    }

	    /* printf("#dx=%g dy=%g dxn=%g dyn=%g\n",dx,dy,dxn,dyn); */
	    /* check for co-linear reversal of path */
	    if ((fabs(dx+dxn)<EPS && fabs(dy+dyn)<EPS)) {
		if (debug) printf("# in 20\n"); 
		xa = xc; ya = yc;
		xd = xb; yd = yb;
	    } else {
		if (debug) printf("# in 21\n"); 
		xa = xb; ya = yb;
		xd = xc; yd = yc;
	    }
	} else {		/* width == 0 */
	    jump(bb,mode);
	    draw(x1,y1, bb, mode);
	    draw(x2,y2, bb, mode);
	    jump(bb,mode);
	}

	temp = temp->next;
	segment++;

    } while(temp != NULL);
}

/* ADD Omask [.cname] [@sname] [:Wwidth] [:Rres] coord coord coord   */

void do_oval(def, bb, mode)
DB_DEFLIST *def;
BOUNDS *bb;
int mode;
{
    NUM x1,y1,x2,y2,x3,y3;

    /* printf("# rendering oval (not implemented)\n"); */

    x1=def->u.o->x1;	/* focii #1 */
    y1=def->u.o->y1;

    x2=def->u.o->x2;	/* focii #2 */
    y2=def->u.o->y2;

    x3=def->u.o->x3;	/* point on curve */
    y3=def->u.o->y3;

}

/* ADD Pmask [.cname] [@sname] [:Wwidth] coord coord coord [coord ...]  */ 

void do_poly(def, bb, mode)
DB_DEFLIST *def;
BOUNDS *bb;
int mode;
{
    COORDS *temp;
    int debug=0;
    int npts=0;
    double x1, y1;
    double x2, y2;
    
    if (debug) printf("# rendering polygon\n"); 

    if (debug) printf("# setting pen %d\n", def->u.p->layer); 
    set_layer(def->u.p->layer, POLY);

    
    /* FIXME: should eventually check for closure here and probably */
    /* at initial entry of points into db.  If last point != first */
    /* point, then the polygon should be closed automatically and an */
    /* error message generated */

    temp = def->u.p->coords;
    x1=temp->coord.x; y1=temp->coord.y;

    if (mode==D_RUBBER) {
    	xwin_draw_circle(x1,y1);
    }

    jump(bb,mode);
    startpoly(bb,mode);
    while(temp != NULL) {
	x2=temp->coord.x; y2=temp->coord.y;
	if (mode==D_RUBBER) {
	    xwin_draw_circle(x2,y2);
	}
	if (npts && temp->prev->coord.x==x2 && temp->prev->coord.y==y2) {
	    ;
	} else {
	    npts++;
	    draw(x2, y2, bb, mode);
	}
	temp = temp->next;
    }
    
    /* when rubber banding, it is important not to redraw over a */
    /* line because it will erase it.  This can occur when only */
    /* two points have been drawn so far.  In this case, we should */
    /* not automatically close the polygon.  So we only execute the  */
    /* next bit of code when npts > 2 */

    if ((x1 != x2 || y1 != y2) && npts > 2) {
        if (mode==D_RUBBER) {
	    xwin_draw_circle(x1,y1);
	}
	draw(x1, y1, bb, mode);
    }
    endpoly(bb,mode);
}


/* ADD Rmask [.cname] [@sname] [:Wwidth] coord coord  */

void do_rect(def, bb, mode)
DB_DEFLIST *def;
BOUNDS *bb;
int mode;	/* drawing mode */
{
    double x1,y1,x2,y2;
    COORDS *cp;
    DB_DEFLIST *dp;
    struct db_line *lp;

    /* printf("# rendering rectangle\n"); */

    x1 = (double) def->u.r->x1;
    y1 = (double) def->u.r->y1;
    x2 = (double) def->u.r->x2;
    y2 = (double) def->u.r->y2;
    
    jump(bb,mode);

    set_layer(def->u.r->layer, RECT);

    if (def->u.r->opts->width == 0.0) {
	startpoly(bb,mode);
	draw(x1, y1, bb, mode); draw(x1, y2, bb, mode);
	draw(x2, y2, bb, mode); draw(x2, y1, bb, mode);
	draw(x1, y1, bb, mode);
	endpoly(bb,mode);
	jump(bb,mode);
    } else { 
    
        /* render a rectangle with width using the line drawing routine */
	dp = (struct db_deflist *) emalloc(sizeof(struct db_deflist));
	dp->next = NULL;
	dp->prev = NULL;
	dp->type = LINE;
        lp = (struct db_line *) emalloc(sizeof(struct db_line));
	dp->u.l = lp;
	dp->u.l->layer=def->u.r->layer;
	dp->u.l->opts=opt_create();
        dp->u.l->opts->width = def->u.r->opts->width;

	cp = coord_new(x1, (y1+y2)/2.0);
	dp->u.l->coords=cp;

	coord_append(cp, x1, y2);
	coord_append(cp, x2, y2);
	coord_append(cp, x2, y1);
	coord_append(cp, x1, y1);
	coord_append(cp, x1, (y1+y2)/2.0);
    	do_line(dp, bb, mode);

	db_free_component(dp);		/* now destroy it */
    }

    if (mode==D_RUBBER) {
    	xwin_draw_circle(x1,y1);
    	xwin_draw_circle(x2,y2);
    }
}


/* ADD Nmask [.cname] [:Mmirror] [:Rrot] [:Yyxratio]
 *             [:Zslant] [:Ffontsize] [:Nfontnum] "string" coord  
 */

void do_note(def, bb, mode)
DB_DEFLIST *def;
BOUNDS *bb;
int mode;
{

    XFORM *xp;

    /* create a unit xform matrix */

    xp = (XFORM *) emalloc(sizeof(XFORM));  
    xp->r11 = 1.0; xp->r12 = 0.0; xp->r21 = 0.0;
    xp->r22 = 1.0; xp->dx  = 0.0; xp->dy  = 0.0;

    /* NOTE: To work properly, these transformations have to */
    /* occur in the proper order, for example, rotation must come */
    /* after slant transformation */

    switch (def->u.n->opts->mirror) {
	case MIRROR_OFF:
	    break;
	case MIRROR_X:
	    xp->r22 *= -1.0;
	    break;
	case MIRROR_Y:
	    xp->r11 *= -1.0;
	    break;
	case MIRROR_XY:
	    xp->r11 *= -1.0;
	    xp->r22 *= -1.0;
	    break;
    }

    mat_scale(xp, def->u.n->opts->font_size, def->u.n->opts->font_size);
    mat_scale(xp, 1.0/def->u.n->opts->aspect_ratio, 1.0);
    mat_slant(xp, def->u.n->opts->slant);
    mat_rotate(xp, def->u.n->opts->rotation);

    jump(bb,mode); set_layer(def->u.n->layer, NOTE);

    xp->dx += def->u.n->x;
    xp->dy += def->u.n->y;

    writestring(def->u.n->text, xp, def->u.n->opts->font_num, bb, mode);

    free(xp);
}

/* ADD Tmask [.cname] [:Mmirror] [:Rrot] [:Yyxratio]
 *             [:Zslant] [:Ffontsize] [:Nfontnum] "string" coord  
 */

void do_text(def, bb, mode)
DB_DEFLIST *def;
BOUNDS *bb;
int mode;
{

    XFORM *xp;

    /* create a unit xform matrix */

    xp = (XFORM *) emalloc(sizeof(XFORM));  
    xp->r11 = 1.0; xp->r12 = 0.0; xp->r21 = 0.0;
    xp->r22 = 1.0; xp->dx  = 0.0; xp->dy  = 0.0;

    /* NOTE: To work properly, these transformations have to */
    /* occur in the proper order, for example, rotation must come */
    /* after slant transformation or else it wont work properly */

    switch (def->u.t->opts->mirror) {
	case MIRROR_OFF:
	    break;
	case MIRROR_X:
	    xp->r22 *= -1.0;
	    break;
	case MIRROR_Y:
	    xp->r11 *= -1.0;
	    break;
	case MIRROR_XY:
	    xp->r11 *= -1.0;
	    xp->r22 *= -1.0;
	    break;
    }

    mat_scale(xp, def->u.t->opts->font_size, def->u.t->opts->font_size);
    mat_scale(xp, 1.0/def->u.t->opts->aspect_ratio, 1.0);
    mat_slant(xp, def->u.t->opts->slant);
    mat_rotate(xp, def->u.t->opts->rotation);

    jump(bb,mode); set_layer(def->u.t->layer, TEXT);

    xp->dx += def->u.t->x;
    xp->dy += def->u.t->y;

    writestring(def->u.t->text, xp, def->u.t->opts->font_num, bb, mode);

    free(xp);
}

/***************** coordinate transformation utilities ************/

XFORM *compose(xf1, xf2)
XFORM *xf1, *xf2;
{
     XFORM *xp;
     xp = (XFORM *) emalloc(sizeof(XFORM)); 

     xp->r11 = (xf1->r11 * xf2->r11) + (xf1->r12 * xf2->r21);
     xp->r12 = (xf1->r11 * xf2->r12) + (xf1->r12 * xf2->r22);
     xp->r21 = (xf1->r21 * xf2->r11) + (xf1->r22 * xf2->r21);
     xp->r22 = (xf1->r21 * xf2->r12) + (xf1->r22 * xf2->r22);
     xp->dx  = (xf1->dx  * xf2->r11) + (xf1->dy  * xf2->r21) + xf2->dx;
     xp->dy  = (xf1->dx  * xf2->r12) + (xf1->dy  * xf2->r22) + xf2->dy;

     return (xp);
}

/* in-place rotate a transform matrix by theta degrees */
void mat_rotate(xp, theta)
XFORM *xp;
double theta;
{
    double s,c,t;
    s=sin(2.0*M_PI*theta/360.0);
    c=cos(2.0*M_PI*theta/360.0);

    t = c*xp->r11 - s*xp->r12;
    xp->r12 = c*xp->r12 + s*xp->r11;
    xp->r11 = t;

    t = c*xp->r21 - s*xp->r22;
    xp->r22 = c*xp->r22 + s*xp->r21;
    xp->r21 = t;

    t = c*xp->dx - s*xp->dx;
    xp->dy = c*xp->dy + s*xp->dx;
    xp->dx = t;
}

/* in-place scale transform */
void mat_scale(xp, sx, sy) 
XFORM *xp;
double sx, sy;
{
    xp->r11 *= sx;
    xp->r12 *= sy;
    xp->r21 *= sx;
    xp->r22 *= sy;
    xp->dx  *= sx;
    xp->dy  *= sy;
}

/* in-place slant transform (for italics) */
void mat_slant(xp, theta) 
XFORM *xp;
double theta;
{
    double s,c;
    double a;

    int debug = 0;
    if (debug) printf("in mat_slant with theta=%g\n", theta);
    if (debug) mat_print(xp);

    s=sin(2.0*M_PI*theta/360.0);
    c=cos(2.0*M_PI*theta/360.0);
    a=s/c;

    xp->r21 += a*xp->r11;
    xp->r22 += a*xp->r12;

    if (debug) mat_print(xp);
}

void mat_print(xa)
XFORM *xa;
{
     printf("\n");
     printf("\t%g\t%g\t%g\n", xa->r11, xa->r12, 0.0);
     printf("\t%g\t%g\t%g\n", xa->r21, xa->r22, 0.0);
     printf("\t%g\t%g\t%g\n", xa->dx, xa->dy, 1.0);
}   


void show_list(currep, layer) 
DB_TAB *currep;
int layer;
{
    int i,j;

    if (currep == NULL) return;

    printf("\n");
    i=layer;
    printf("%d ",i);
    for (j=((sizeof(i)*8)-1); j>=0; j--) {
	if (getbits(currep->show[i], j, 1)) {
	    printf("1");
	} else {
	    printf("0");
	}
    }
    printf("\n");
}

int getbits(x,p,n)	/* get n bits from position p */ 
unsigned int x, p, n;
{
    return((x >> (p+1-n)) & ~(~0 << n));
}

void show_init(DB_TAB *currep) { /* set everyone visible, but RO */
    int i;

    if (currep == NULL) return;

    /* default is all visible, none modifiable. */
    for (i=0; i<MAX_LAYER; i++) {
    	currep->show[i]=0|ALL;  /* visible */
    }
}

void show_set_visible(DB_TAB *currep, int comp, int layer, int state) {

    int lstart, lstop, i;
    int debug=0;

    if (currep == NULL) return;

    if (layer==0) {
    	lstart=0;
	lstop=MAX_LAYER;
    } else {
    	lstart=layer;
	lstop=layer;
    }

    for (i=lstart; i<=lstop; i++) {
	if (state) {
	    currep->show[i] |= comp;
	} else {
	    currep->show[i] &= ~(comp);
	}
    }
     
    if (debug) printf("setting layer %d, comp %d, state %d db %d\n",
    	layer, comp, state,  show_check_visible(currep, comp, layer));
}

void show_set_modify(DB_TAB *currep, int comp, int layer, int state) {
    int lstart, lstop, i;
    int debug=0;

    if (currep == NULL) return;

    if (layer==0) {
    	lstart=0;
	lstop=MAX_LAYER;
    } else {
    	lstart=layer;
	lstop=layer;
    }

    for (i=lstart; i<=lstop; i++) {
	if (state) {
	    currep->show[i] |= (comp*2);
	} else {
	    currep->show[i] &= ~(comp*2);
	}
    }

    if (debug) printf("setting layer %d, comp %d, state %d db %d\n",
    	layer, comp, state,  show_check_modifiable(currep, comp, layer));
}

/* check modifiability */
int show_check_modifiable(DB_TAB *currep, int comp, int layer) {

    int debug=0;

    if (currep == NULL) return(0);

    if (debug) show_list(currep, layer);

    return (currep->show[layer] & (comp*2));
}

/* check visibility */
int show_check_visible(DB_TAB *currep, int comp, int layer) {

    int debug=0;
    if (debug) show_list(currep, layer);

    if (currep == NULL) return(1);

    if (debug) printf("checking vis, comp %d, layer %d, returning %d\n",
    	comp, layer, currep->show[layer] & comp);

    return (currep->show[layer] & (comp));
}

