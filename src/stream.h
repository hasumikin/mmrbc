#ifndef PICORBC_FMEMSTREAM_H_
#define PICORBC_FMEMSTREAM_H_

typedef enum stream_type
{
  STREAM_TYPE_FILE,
  STREAM_TYPE_MEMORY
} StreamType;

typedef struct stream_interface
{
  StreamType type;
  uint8_t node_box_size;
  void *stream;
  char *(*fgetsProc)(char *s, int n, FILE *stream);
  int (*feofProc)(FILE *stream);
} StreamInterface;

typedef struct fmemstream
{
  int pos;
  char *mem;
  int size;
} fmemstream;

StreamInterface *StreamInterface_new(char *c, StreamType st);

void StreamInterface_free(StreamInterface *si);

#endif /* PICORBC_FMEMSTREAM_H_ */
