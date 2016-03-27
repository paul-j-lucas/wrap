/**
 * From Wikipedia:
 * The data link escape character (DLE) was intended to be a signal to the other end of a data link
 * that the following character
 * is a control character such as STX or ETX.
 * For example a packet may be structured in the following way
 * (DLE)(STX)(PAYLOAD)(DLE)(ETX).
 *
 * Wrap and Wrapc use it to signal the start of an interprocess message
 * between them.
 */
#define ASCII_DLE                 '\x10'
