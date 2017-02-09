static size_t       prefix_len0;        // length of prefix_buf
static line_buf_t   suffix_buf;     // characters stripped/appended
static size_t       suffix_len;              // length of suffix_buf
//
// Two pipes:
// + pipes[0][0] -> wrap(1)                   [child 2]
//           [1] <- read_source_write_wrap()  [child 1]
// + pipes[1][0] -> read_wrap()               [parent]
//           [1] <- wrap(1)                   [child 2]
//
static int          pipes[2][2];

#define CURR        input_buf.dl_curr        /* current line */
#define NEXT        input_buf.dl_next              /* next line */
