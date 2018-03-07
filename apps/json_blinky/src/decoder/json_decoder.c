#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <json/json.h>
#include "json_utils.h"
#include "console/console.h"

int fetch_map(const char *map);

char *response;

#define MAXCHANNELS 72

struct sat_data_struct {
    long long int PRN;
    long long int elevation;
    long long int azimuth;
    bool usedflags;
};

static struct sat_data_struct sat_data[MAXCHANNELS];

struct jbuf tjb;

int visible;

const struct json_attr_t sat_attrs[] = {
    {	.attribute = "PRN",
    	.type = t_integer,
    	JSON_STRUCT_OBJECT(struct sat_data_struct, PRN)
    },
    {	.attribute = "el",
    	.type = t_integer,
    	JSON_STRUCT_OBJECT(struct sat_data_struct, elevation)
    },
    {
    	.attribute = "az",
    	.type = t_integer,
    	JSON_STRUCT_OBJECT(struct sat_data_struct, azimuth)
    },
    {
    	.attribute = "used",
    	.type = t_boolean,
    	JSON_STRUCT_OBJECT(struct sat_data_struct, usedflags)
    }
};

const struct json_attr_t json_attrs_sky[] = {
    {
    	.attribute = "class",
        .type = t_check,
        .dflt.check = "SKY"
    },
    {
    	.attribute = "satellites",
    	.type = t_array,
    	JSON_STRUCT_ARRAY(sat_data, sat_attrs, &visible),
    }
};

int fetch_map(const char *map){
    int i;
    int rc;

    buf_init(&tjb,(char *) map);
    console_printf("Buffer Initiated\n");

    rc = json_read_object(&tjb.json_buf, json_attrs_sky);

    console_printf("JSON Read rc=%d\n", rc);
    console_printf("JSON visible %d\n", visible);

    for (i = 0; i < visible; i++){
        console_printf("PRN = %lld, elevation = %lld, azimuth = %lld used = %d\n",
                       sat_data[i].PRN, sat_data[i].elevation,
                       sat_data[i].azimuth, sat_data[i].usedflags);
    }
    printf("Complete\n");
    return 1;
}
