#include "jsmn.h"
//#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fatParse.h"
#include "PegnetParse.h"

int processFat1Tx(const char *inputaddress,int r, jsmntok_t *t, int8_t *d, uint32_t length, txContent_t *content);
/*
 * FAT 0/1 Json Parser
 */

static int jsoneq(const int8_t *json, jsmntok_t *tok, const char *s) {
  if (tok->type == JSMN_STRING && (int)strlen(s) == tok->end - tok->start &&
      strncmp((char*)&json[tok->start], s, tok->end - tok->start) == 0) {
    return 0;
  }
  return -1;
}

static int jsonpartialeq(const int8_t *json, jsmntok_t *tok, const char *s) {
  if (tok->type == JSMN_STRING &&
      strncmp((char*)&json[tok->start], s, MIN((uint32_t)(tok->end - tok->start),strlen(s))) == 0) {
    return 0;
  }
  return -1;
}

/*
* JSMN_OBJECT  //whole tx
* JSMN_STRING  //input/output/metadata
* JSMN_OBJECT  //input object
* JSMN_STRING  //input address
* JSMN_PRIMITIVE //value
* JSMN_STRING  //output
* JSMN_OBJECT  //output object
* JSMN_STRING  //output address
* JSMN_PRIMITIVE //output value
*/

const int MINIMUM_VIABILITY = 10;

//JSMN_ERROR_PART ==


int processFat0Outputs(jsmntok_t **tt, jsmntok_t *tend, int8_t *d,uint32_t length,txContent_t *content)
{
    jsmntok_t *t = *tt;

    if ( t == tend )
    {
        return FAT_INSUFFICIENT_IOM;
    }
    if ( t->type != JSMN_OBJECT )
    {
        return FAT_ERROR_OUTPUT_OBJECT_EXPECTED;
    }

    ++t;

    content->header.outputcount = 0;

    while(jsonpartialeq(d, t, "FA1" ) == 0 ||
          jsonpartialeq(d, t, "FA2" ) == 0 ||
          jsonpartialeq(d, t, "FA3" ) == 0 )
    {

        if ( content->header.outputcount > MAX_OUTPUT_ADDRESSES)
        {
            return FAT_ERROR_OUTPUT_ADDRESS_EXPECTED;
        }

        if ( (uint32_t)(t->end - t->start) != fct_address_length )
        {
            return FAT_ERROR_INVALID_FCT_ADDRESS;
        }

        ++t;

        if ( t->type != JSMN_PRIMITIVE )
        {
            return FAT_ERROR_OUTPUT_AMOUNT_EXPECTED;
        }


        //store off the pointer to the address
        content->outputs[content->header.outputcount].addr.fctaddr = d + (t-1)->start;

        //int32_t val = toString(d+t->start, t->end - t->start);

        //convert to fatoshis
        content->outputs[content->header.outputcount].amt.fat.entry = &d[t->start];
        content->outputs[content->header.outputcount].amt.fat.size = t->end - t->start;

        ++t;

        ++content->header.outputcount;

    }
    *tt=t;
    return 0;
}


int processFat0Inputs(const char *inputaddress, jsmntok_t **tt, jsmntok_t *tend, int8_t *d,uint32_t length, txContent_t *content)
{
    jsmntok_t *t = *tt;
    if ( t == tend )
    {
        return FAT_INSUFFICIENT_IOM;
    }
    if ( t->type != JSMN_OBJECT )
    {
        return FAT_ERROR_INPUT_OBJECT_EXPECTED;
    }

    ++t;

    if ( t->type != JSMN_STRING )
    {
        return FAT_ERROR_INPUT_ADDRESS_EXPECTED;
    }


    content->header.inputcount = 0;

    while(jsonpartialeq(d, t, "FA1" ) == 0 ||
          jsonpartialeq(d, t, "FA2" ) == 0 ||
          jsonpartialeq(d, t, "FA3" ) == 0 )
    {
        if ( content->header.inputcount > MAX_INPUT_ADDRESSES)
        {
            return FAT_ERROR_INPUT_TOO_MANY_ADDRESS;
        }

        if ( (uint32_t)(t->end - t->start) != fct_address_length )
        {
            return FAT_ERROR_INVALID_FCT_ADDRESS;
        }

	
	bool haveinputs = false;
        if( jsoneq(d,t,inputaddress) == 0 )
        {

            haveinputs = true;
            //store off the pointer to the address
            //db: test not storing inputs since we don't need them
            content->inputs[content->header.inputcount].addr.fctaddr = d + t->start;
	}
        ++t;

        if ( t->type != JSMN_PRIMITIVE )
        {
            return FAT_ERROR_INPUT_AMOUNT_EXPECTED;
        }





        //int32_t val = toString(d+t->start, t->end - t->start);

        //convert to fatoshis
        //db: test not storing inputs since we don't need them
	if ( haveinputs )
	{
            content->inputs[content->header.inputcount].amt.fat.entry = &d[t->start];
            content->inputs[content->header.inputcount].amt.fat.size = t->end - t->start;//val * 100000000ul;
	}


//        fprintf(stderr,"- Value: %.*s %ld \n", 52,
//               content->inputs[content->header.inputcount].addr.fctaddr,content->inputs[content->header.inputcount].value / 100000000 );

        ++t;

        if( haveinputs )
        {

            ++content->header.inputcount;
	}
    }

    *tt = t;
    return 0;
}


int processFat0Tx(const char *inputaddress, int r, jsmntok_t *t, int8_t *d, uint32_t length, txContent_t *content)
{
    //minimum viability for a fat transaction is 10 tokens
    if (r < 1 || t->type != JSMN_OBJECT) {
      //printf("Object expected\n");
      return FAT_INSUFFICIENT_IOM;
    }


    jsmntok_t *tend = &t[r-1];
    ++t;


    if ( t == tend )
    {
        return FAT_INSUFFICIENT_IOM;
    }

    int ret = 0;
    for ( ; t < tend;  )
    {
        if ( t->type != JSMN_STRING )
        {
            return tend - t; //FAT_ERROR_INPUT_EXPECTED;
        }

        if (jsoneq((int8_t*)d, t, "inputs") == 0)
        {
            ++t;

            ret = processFat0Inputs(inputaddress,&t, tend, d, length,content);
            if ( ret )
            {
                return ret;
            }
        }
        else if (jsoneq((int8_t*)d, t, "outputs") == 0)
        {
            ++t;
            ret = processFat0Outputs(&t, tend, d, length, content);
            if ( ret )
            {
                return ret;
            }
        }
        else //metadata or something else, don't care
        {
            ++t;
            continue;
        }
    }
    return 0;
}

int isSpace(char c)
{
    switch (c) {
        case ' ':
        case '\t':
        case '\n':
        case '\v':
        case '\f':
        case '\r':
            return 1;
    }
    return 0;
}

int parseFatTxContent(int fattype, const char *inputaddress, int8_t *d, uint32_t length, txContent_t *content)
{
    int8_t *de = d;
    uint8_t valid = 0;
    uint32_t i = 0;
    jsmntok_t *t = content->t.fat;
    os_memset(content->t.fat, 0, sizeof(content->t.fat));
    int maxtokens = sizeof(content->t.fat)/sizeof(content->t.fat[0]);

    for(i = 0; i < length; ++i )
    {
        if ( d[i] == '{')
        {
            //input better be next...
            for (uint32_t j = i+1; j < length; ++j )
            {
                if ( !isSpace(d[j]) )
                {
                    static char *ver   = "\"versi";
                    static char *input = "\"input";
                    static char *outpu = "\"outpu";
                    static char *metad = "\"metad";
                    if ( strlen(input) > length-j )
                    {
                        continue;
                    }

                    if ( strncmp((char*)&d[j], input, strlen(input)) == 0 ||
                         strncmp((char*)&d[j], outpu, strlen(outpu)) == 0 ||
                         strncmp((char*)&d[j], metad, strlen(metad)) == 0 ||
                         strncmp((char*)&d[j], ver, strlen(ver)) == 0 )
                    {
                        valid = 1;
                    }
                    break;
                }
            }
            if ( valid )
            {
                break;
            }
        }
    }

    if (valid == 0 )
    {
        return FAT_ERROR_INVALID_JSON;
    }

    de = &d[i];

    int r;
    jsmn_parser p;

    //jsmntok_t *curtok = t;


    jsmn_init(&p);


    r = jsmn_parse(&p, (char*)de, length, t,maxtokens);


    if (r < 0)
    {
        //printf("Failed to parse JSON: %d\n", r);
        return FAT_ERROR_INVALID_JSON;
    }

    /* Assume the top-level element is an object */
    if (r < 1 || t[0].type != JSMN_OBJECT)
    {
        //printf("Object expected\n");
        return FAT_ERROR_JSON_OBJECT_NOT_FOUND;
    }

    int ret = 0;
    switch ( fattype )
    {
    case 0:
        ret = processFat0Tx(inputaddress,r,t, de, length,content);
        break;
    case 1:
        ret = processFat1Tx(inputaddress,r,t, de, length,content);
        break;
    case 2:
        ret = processPegTx(inputaddress, r,t, de, length,content);
        break;
    default:
        ret = FAT_ERROR_UNSUPPORTED_FAT_TYPE ;

    };
    return ret;
}

int processFat1Outputs(jsmntok_t **tt, jsmntok_t *tend, int8_t *d,uint32_t length,txContent_t *content)
{
    jsmntok_t *t = *tt;

    if ( t == tend )
    {
        return FAT_INSUFFICIENT_IOM;
    }
    if ( t->type != JSMN_OBJECT )
    {
        return FAT_ERROR_OUTPUT_OBJECT_EXPECTED;
    }

    ++t;

    content->header.outputcount = 0;

        while(jsonpartialeq((int8_t*)d, t, "FA1" ) == 0 ||
          jsonpartialeq((int8_t*)d, t, "FA2" ) == 0 ||
          jsonpartialeq((int8_t*)d, t, "FA3" ) == 0 )
    {

        if ( content->header.outputcount > MAX_OUTPUT_ADDRESSES)
        {
            return FAT_ERROR_OUTPUT_ADDRESS_EXPECTED;
        }

        if ( (uint32_t)(t->end - t->start) != fct_address_length )
        {
            return FAT_ERROR_INVALID_FCT_ADDRESS;
        }

        ++t;

        if ( t->type != JSMN_ARRAY )
        {
            return FAT_ERROR_OUTPUT_AMOUNT_EXPECTED;
        }


        //store off the pointer to the address
        content->outputs[content->header.outputcount].addr.fctaddr = (int8_t*)(&d[(t-1)->start]);

        //int32_t val = toString(d+t->start, t->end - t->start);

        //convert to fatoshis
        content->outputs[content->header.outputcount].amt.fat.entry = (int8_t*)&d[t->start];
        content->outputs[content->header.outputcount].amt.fat.size = t->end - t->start;// val * 100000000ul;

//        fprintf(stderr,"- Value: %.*s %ld \n", 52,
//               content->outputs[content->header.outputcount].addr.fctaddr,content->outputs[content->header.outputcount].value / 100000000 );

        int jump = t->end;
        while ( t->start < jump && t <= tend  )
        {
            ++t;
        }

        ++content->header.outputcount;

    }
    *tt=t;
    return 0;
}

int processFat1Inputs(const char *inputaddress, jsmntok_t **tt, jsmntok_t *tend, int8_t *d,uint32_t length, txContent_t *content)
{
    jsmntok_t *t = *tt;
    if ( t == tend )
    {
        return FAT_INSUFFICIENT_IOM;
    }
    if ( t->type != JSMN_OBJECT )
    {
        return FAT_ERROR_INPUT_OBJECT_EXPECTED;
    }

    ++t;

    if ( t->type != JSMN_STRING )
    {
        return FAT_ERROR_INPUT_ADDRESS_EXPECTED;
    }


    content->header.inputcount = 0;

    while(jsonpartialeq((int8_t*)d, t, "FA1" ) == 0 ||
          jsonpartialeq((int8_t*)d, t, "FA2" ) == 0 ||
          jsonpartialeq((int8_t*)d, t, "FA3" ) == 0 )
    {
        if ( content->header.inputcount > MAX_INPUT_ADDRESSES)
        {
            return FAT_ERROR_INPUT_TOO_MANY_ADDRESS;
        }

        if ( (uint32_t)(t->end - t->start) != fct_address_length )
        {
            return FAT_ERROR_INVALID_FCT_ADDRESS;
        }

	bool haveinput = false;
        if( jsoneq(d,t,inputaddress) == 0 )
        {
            haveinput = true;

            //store off the pointer to the address
            //db: test not storing inputs since we don't need them
            content->inputs[content->header.inputcount].addr.fctaddr = (int8_t*)(&d[t->start]);
	}
        ++t;

        //expect an array
        if ( t->type != JSMN_ARRAY )
        {
            return FAT_ERROR_INPUT_AMOUNT_EXPECTED;
        }


        //convert to fatoshis
        //db: test not storing inputs since we don't need to show them?
	if( haveinput )
        {

            content->inputs[content->header.inputcount].amt.fat.entry = (int8_t*)&d[t->start];
            content->inputs[content->header.inputcount].amt.fat.size = t->end - t->start;//val * 100000000ul;
	}


        int jump = t->end;
        while ( t->start < jump && t <= tend )
        {
            ++t;
        }

	if ( haveinput )
	{
            ++content->header.inputcount;
	}
    }

    *tt = t;
    return 0;
}


int processFat1Tx(const char *inputaddress,int r, jsmntok_t *t, int8_t *d, uint32_t length, txContent_t *content)
{
    //minimum viability for a fat transaction is 10 tokens
    if (r < 1 || t->type != JSMN_OBJECT) {
      //printf("Object expected\n");
      return FAT_INSUFFICIENT_IOM;
    }


    jsmntok_t *tend = &t[r-1];
    ++t;


    if ( t == tend )
    {
        return FAT_INSUFFICIENT_IOM;
    }

    int ret = 0;
    for ( ; t < tend;  )
    {
        if ( t->type != JSMN_STRING )
        {
            return FAT_ERROR_INPUT_EXPECTED;//tend - t; //FAT_ERROR_INPUT_EXPECTED;
        }

        if (jsoneq((int8_t*)d, t, "inputs") == 0)
        {
            ++t;

            ret = processFat1Inputs(inputaddress,&t, tend, d, length,content);
            if ( ret )
            {
                return ret;
            }
        }
        else if (jsoneq((int8_t*)d, t, "outputs") == 0)
        {
            ++t;
            ret = processFat1Outputs(&t, tend, d, length, content);
            if ( ret )
            {
                return ret;
            }
        }

        else if (jsoneq((int8_t*)d, t, "metadata") == 0)
        {
            ++t;
            //consume the metadata... don't really care what it says.
            int jump = t->end;
            while ( t->start < jump && t <= tend )
            {
                ++t;
            }

        }
        else //this may eventually cause a problem and error out if we get here.
        {
            ++t;
            continue;
        }
    }
    return 0;
}


int parseFatTx(int fattype, char *inputaddress, int8_t *d, uint32_t length,
                       txContent_t *content) {
    int result = 0;
    initContent(content);
    result = parseFatTxContent(fattype, inputaddress, d,length, content);
    return result;
            /*
    BEGIN_TRY {
        TRY {
            initContent(content);
            result = parseTxInternal(data, length, content);
            return USTREAM_FINISHED;
        }
        CATCH_OTHER(e) {
            result = USTREAM_FAULT;
        }
        FINALLY {
        }
    }
    END_TRY;
    */
    return result;
}
