#include "json/json.h"

#define JSON_BIGBUF_SIZE    192

/* a test structure to hold the json flat buffer and pass bytes
 * to the decoder */
struct jbuf {
    /* json_buffer must be first element in the structure */
    struct json_buffer json_buf;
    char * start_buf;
    char * end_buf;
    int current_position;
};

int fetch_map(const char *map);
char jbuf_read_next(struct json_buffer *jb);
char jbuf_read_prev(struct json_buffer *jb);
int jbuf_readn(struct json_buffer *jb, char *buf, int size);
int write(void *buf, char* data, int len);
void buf_init(struct jbuf *ptjb, char *string);
