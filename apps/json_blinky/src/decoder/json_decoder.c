#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <json/json.h>
#include "json_utils.h"
#include "console/console.h"

int fetch_map(const char *map);

char *response;

#define MAXCHANNELS 72

static long long int PRN[MAXCHANNELS];
static long long int elevation[MAXCHANNELS];
static long long int azimuth[MAXCHANNELS];
static bool usedflags[MAXCHANNELS];
struct jbuf tjb;

int visible;

const struct json_attr_t sat_attrs[] = {
    {	.attribute = "PRN",
    	.type = t_integer,
    	.addr.integer = PRN
    },
    {	.attribute = "el",
    	.type = t_integer,
    	.addr.integer = elevation
    },
    {
    	.attribute = "az",
    	.type = t_integer,
    	.addr.integer = azimuth
    },
    {
    	.attribute = "used",
    	.type = t_boolean,
    	.addr.boolean = usedflags
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
    	.addr.array = {
            .element_type = t_structobject,
            .arr.objects.subtype=sat_attrs,
            .maxlen = MAXCHANNELS,
            .count = &visible
        }
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
                       PRN[i], elevation[i], azimuth[i], usedflags[i]);
    }
    printf("Complete\n");
    return 1;
}
