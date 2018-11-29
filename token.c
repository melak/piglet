#include <stdio.h>
#include <string.h>             /* for strchr() */
#include <ctype.h>
#include <math.h>

#include "token.h"
#include "eprintf.h"
#include "db.h"
#include "xwin.h"
#include "rlgetc.h"
#include "ev.h"
#include "expr.h"	/* for evaluating expression */

static char promptbuf[128];

void token_set_mode(LEXER *lp, int mode) {
    lp->parse = mode;
}

LEXER *token_stream_open(FILE *fp, char *name)  {
    LEXER *lp;
    lp = (LEXER *) emalloc(sizeof(struct lexer));
    lp->name = strsave(name);
    lp->word[0] = '\0';	/* place to store token return value */
    lp->bufp = 0;  	/* no tokens in token pushback buf */
    lp->token_stream = fp;
    lp->pbufp = 0;	/* no characters in pushback buffer */
    lp->mode = MAIN;
    lp->line = 1;	/* keep track of line number for stream */
    lp->parse = 0;	/* parsing mode: 0=normal, 1=raw */
    return (lp);
}

int token_err(char *modulename, LEXER *lp, char *expected, TOKEN token) 
{
    if (strcmp(lp->name, "STDIN") == 0) {
	printf("%s: %s, got %s\n", modulename, expected, tok2str(token));
    } else {
	printf("%s: in %s, line %d: %s, got %s\n", 
	    modulename, lp->name, lp->line, expected, tok2str(token));
    }
    return(0);
}

int token_stream_close(LEXER *lp)  {
    int retcode;
    retcode=fclose(lp->token_stream);
    free(lp->name);
    free(lp);
    return(retcode);
}

/* lookahead to see the next token */
TOKEN token_look(LEXER *lp, char **word)
{
    TOKEN token;
    token=token_get(lp, word);
    token_unget(lp, token, *word);
    return token;
}

/* stuff back a token */
int token_unget(LEXER *lp, TOKEN token, char *word) 
{
    int debug=0;
    if (debug) printf("ungetting %s\n", word);
    if (lp->bufp >= BUFSIZE) {
	eprintf("ungettoken: too many characters");
	return(-1);
    } else {
	lp->tokbuf[lp->bufp].word = (char *) estrdup(word);
	lp->tokbuf[lp->bufp++].tok = token;
	return(0);
    }
}

int token_flush_EOL(LEXER *lp) 
{
    char *word;
    TOKEN token;
    while ((token=token_get(lp, &word) != EOL) && (token != EOF)) {
	;
    }
    return(0);
}

TOKEN token_get(LEXER *lp, char **word) /* collect and classify token */
{
    enum {NEUTRAL,INQUOTE,INWORD,INOPT,INNUM,INVAR,INEXPR,COMMENT} state = NEUTRAL;
    int c,d,retval;
    char *w;
    int debug=0;
    char *str;
    double val;
    int parencount=0;

    *word = lp->word;		/* set up token text return value */

    if (lp->bufp > 0) {		/* characters in pushback buffer */
	strcpy(*word, lp->tokbuf[--(lp->bufp)].word);
	if ((lp->tokbuf[lp->bufp].word) != NULL) {
	    free(lp->tokbuf[lp->bufp].word);		// FIXME: have seen crashes here...
	}
	lp->tokbuf[lp->bufp].word = (char *) NULL;
	return(lp->tokbuf[lp->bufp].tok);
    }

    w=lp->word;
    if (lp->parse) {	/* raw mode */
	*w++ = (char) procXevent();
	*w = '\0';
	return(RAW);	/* simply bypass readline */
    }

    w=lp->word;
    while((c=rlgetc(lp)) != EOF) {
	switch(state) {
	    case NEUTRAL:
		switch(c) {
		    case ' ':
		    case '\t':
			continue;
		    case '\n':
		    case '\r':
			*w++ = c;
			*w = '\0';
			if (debug) printf("returning EOL: %s \n", lp->word);
			lp->line++;
			return(EOL);
		    case ',':
			*w++ = c;
			*w = '\0';
			if (debug) printf("returning COMMA: %s \n", lp->word);
			return(COMMA);
		    case '"':
			state = INQUOTE;
			strcpy(promptbuf, rl_saveprompt());
			strcat(promptbuf, "(in quote)> ");
			rl_setprompt(promptbuf);
			continue;
		    case '^':
			*w++ = c;
			*w = '\0';
			if (debug) printf("returning BACK: %s \n", lp->word);
			return(BACK);
			continue;
		    case ':':
			state = INOPT;
			*w++ = c;
			continue;
		    case ';':
			*w++ = c;
			*w = '\0';
			if (debug) printf("returning EOC: %s \n", lp->word);
			return(EOC);
		    case '0':
		    case '1':
		    case '2':
		    case '3':
		    case '4':
		    case '5':
		    case '6':
		    case '7':
		    case '8':
		    case '9':
			*w++ = c;
			state = INNUM;
			continue;
		    case '+':
		    case '-':
		    case '#':
			*w++ = c;
			c = rlgetc(lp);
			rl_ungetc(lp,c);
			if (isdigit(c) || c=='.') {
			    state = INNUM;
			} else {
			    state = INOPT;
			}
			continue;
		    case '@':
			*w++ = c;
			state = INOPT;
			continue;
		    case '(':
			state = INEXPR;
			parencount=1;
			strcpy(promptbuf, rl_saveprompt());
			strcat(promptbuf, "(in a parenthesized expression)> ");
			rl_setprompt(promptbuf);
			continue;
		    case '$':
			// *w++ = c; 	/* don't save the dollar */
			c = rlgetc(lp);
			if (c=='$') {
			    state = COMMENT;
			} else {
			    rl_ungetc(lp,c);
			    state = INVAR;
			}
			continue;
		    case '.':
			*w++ = c;
			c = rlgetc(lp);
			if (isdigit(c)) {
			    rl_ungetc(lp,c);
			    state = INNUM;
			} else {
			    rl_ungetc(lp,c);
			    state = INOPT;
			}
			continue;	
		    default:
			// printf("in default with c=%d\n", c);
			state = INWORD;
			*w++ = c;
			continue;
		}
	    case INNUM:
		if (isdigit(c) || c=='.') {
		    *w++ = c;
		    continue;

		/* took this out because of ambiguity with mouse */
		/* clicks like "ADD L1MOV", 1MOV became ident rather */
		/* than 1 and MOV being parsed separately */

		/* } else if (isalnum(c) || (c=='_') || (c=='.') ) {
		    *w++ = c;
		    state = INWORD;
		    continue; */

		} else {
		    rl_ungetc(lp,c);
		    *w = '\0';
		    if (debug) printf("returning NUMBER: %s \n", lp->word);
		    return(NUMBER);
		}
	    case INVAR:
		if (isalnum(c)) {
		    *w++ = c;
		    continue;
		} else if (strchr("_+-.",c) != NULL) {
		    *w++ = c;
		    continue;
		} else {
		    rl_ungetc(lp,c);
		    *w = '\0';
        	    if (strncasecmp(lp->word,"FILES",5) == 0) {
		    	return(CMD);
        	    } else if ((str=EVget(lp->word)) == NULL) {
		       lp->word[0]='\0';
		    } else {
		       // rl_ungets(lp, str);
		       strcpy(lp->word, str);	// substitute definition
		    }
    		    // w=lp->word;
		    // *w='\0';
		    // state = NEUTRAL;
		    return(QUOTE);
		}
	    case INOPT:
		if (isalnum(c)) {
		    *w++ = c;
		    continue;
		} else if (strchr("+-.,",c) != NULL) {
		    *w++ = c;
		    continue;
		} else {
		    rl_ungetc(lp,c);
		    *w = '\0';
		    if (debug) printf("returning OPT: %s \n", lp->word);
		    return(OPT);
		}
	    case INEXPR:
		switch(c) {
		    case ')':
			if (--parencount == 0) {
			    *w = '\0';
			    // if (debug) printf("got EXPR: %s \n", lp->word);
			    rl_restoreprompt();
			    retval = parse_exp(&val, lp->word);
			    // printf("got EXPR:\"%s\" %f (%d)\n",lp->word,val,retval);
			    if (retval) { 
				strcpy(lp->word, "BAD-EXPRESSION");
				return(UNKNOWN);
			    } 
			    sprintf(lp->word, "%g", val-fmod(val,1.0/pow(10.0,RES)));
			    return(NUMBER);
			} else {
			    *w++ = c;
			    continue;
			}
		    case '(':
		    	parencount++;
			// fall through
		    default:
			*w++ = c;
			continue;
		}
	    case INQUOTE:
		switch(c) {
		    case '\\':
			/* escape quotes, but pass everything else along */
			if ((d = rlgetc(lp)) == '"') {
			    *w++ = d;
			} else {
			    *w++ = c;
			    *w++ = d;
			}
			continue;
		    case '"':
			*w = '\0';
			if (debug) printf("returning QUOTE: %s \n", lp->word);
			rl_restoreprompt();
			return(QUOTE);
		    default:
			*w++ = c;
			continue;
		}
	    case INWORD:
		if (!isalnum(c) && (c!='_') && (c!='.') && (c!='/') ) {
		    rl_ungetc(lp,c);
		    *w = '\0';
		    // printf("looking up <%s>\n", lp->word);
		    if (lookup_command(lp->word)) {
		        if (debug) printf("lookup returns CMD: %s\n", lp->word);
			return(CMD);
		    } else {
		        if (debug) printf("lookup returns IDENT: %s\n", lp->word);
			return(IDENT);
		    }
		}
		*w++ = c;
		continue;
	    case COMMENT:
		switch(c) {
		    case '\n':
		    case '\r':
			*w++ = c;
			*w = '\0';
			if (debug) printf("returning EOL: %s \n", lp->word);
			lp->line++;
			return(EOL);
		    default:
			continue;
		}
	    default:
		fprintf(stderr,"pig: error in lex loop\n");
		return(UNKNOWN);
		break;
	} /* switch state */
    } /* while loop */
    return(EOF);
}

char *tok2str(TOKEN token)
{
    switch (token) {
	case IDENT: return("IDENT"); 	 break;
	case CMD: return("CMD"); 	 break;
	case QUOTE: return("QUOTE"); 	 break;
	case NUMBER: return("NUMBER"); 	 break;
	case OPT: return("OPT"); 	 break;
	case EOL: return("EOL"); 	 break;
	case EOC: return("EOC"); 	 break;
	case COMMA: return("COMMA"); 	 break;
	case BACK:  return("BACK");	 break;
	case END: return("END"); 	 break;
	case COMMENT: return("COMMENT"); break;
	default: return("UNKNOWN"); 	 break;
    }
}


int getnum(LEXER *lp, char *cmd, double *px, double *py)
{
    int done=0;
    TOKEN token;
    char *word;
    int state = 0;
    int debug = 0;

    if (debug) printf("in getnum\n");
	
    while(!done) {
	token = token_look(lp,&word);
	if (debug) printf("got %s: %s\n", tok2str(token), word);

	switch(state) {	
	case 0:
	    if (token == NUMBER) {
		state = 1;
	    } else if (token == EOL) {
		token_get(lp,&word); 	/* just ignore it */
	    } else {
	    	return(0);
	    }
	    break;
	case 1:		/* get pair of xy coordinates */
	    if (debug) printf("in NUM1\n");
	    if (token == NUMBER) {
		token_get(lp,&word);
		sscanf(word, "%lf", px);	/* scan it in */
		state = 2;
	    } else if (token == EOL) {
		token_get(lp,&word); 	/* just ignore it */
	    } else if (token == EOC || token == CMD) {
		state = 4;	
	    } else {
		token_err(cmd, lp, "expected NUMBER", token);
		state = 4; 
	    }
	    break;
	case 2:		
	    if (debug) printf("in COM1\n");
	    if (token == EOL) {
		token_get(lp,&word); /* just ignore it */
	    } else if (token == COMMA) {
		token_get(lp,&word);
		state = 3;
	    } else if (token == NUMBER) {	/* this line makes the comma optional */
		state = 3;
	    } else {
		token_err(cmd, lp, "expected COMMA", token);
		state = 4;
	    }
	    break;
	case 3:
	    if (debug) printf("in NUM2\n");
	    if (token == NUMBER) {
		token_get(lp,&word);
		sscanf(word, "%lf", py);	/* scan it in */
		return(1);
	    } else if (token == EOL) {
		token_get(lp,&word); 	/* just ignore it */
	    } else if (token == EOC || token == CMD) {
		state = 4;
	    } else {
		token_err(cmd, lp, "expected NUMBER", token);
		state = 4; 
	    }
	    break;
	case 4:		
	    done++;
	}
    }
    return(0);	
}

