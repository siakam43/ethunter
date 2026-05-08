/* CG-Bench fixture: fnptr-global-array/example_3 */
/* fnptr: convert_func[type], targets: convert_str, convert_int, convert_flt, convert_color, convert_timestamp, convert_alignment */

static const char *ass_split_section(ASSSplitContext *ctx, const char *buf)
{
    const ASSSection *section = &ass_sections[ctx->current_section];
    int *number = &ctx->field_number[ctx->current_section];
    int *order = ctx->field_order[ctx->current_section];
    int i, len;

    while (buf && *buf) {
		...
		buf += len + 1;
		for (i=0; !is_eol(*buf) && i < *number; i++) {
			int last = i == *number - 1;
			buf = skip_space(buf);
			len = strcspn(buf, last ? "\r\n" : ",\r\n");
			if (order[i] >= 0) {
				ASSFieldType type = section->fields[order[i]].type;
				ptr = struct_ptr + section->fields[order[i]].offset;
				convert_func[type](ptr, buf, len);
			}
			buf += len;
			if (!last && *buf) buf++;
			buf = skip_space(buf);
		}
	}
	... 
    return buf;
}

static const ASSConvertFunc convert_func[] = {
    [ASS_STR]       = convert_str,
    [ASS_INT]       = convert_int,
    [ASS_FLT]       = convert_flt,
    [ASS_COLOR]     = convert_color,
    [ASS_TIMESTAMP] = convert_timestamp,
    [ASS_ALGN]      = convert_alignment,
};


/* Stub implementation for convert_str */
void convert_str(void) {}



/* Stub implementation for convert_int */
void convert_int(void) {}



/* Stub implementation for convert_flt */
void convert_flt(void) {}



/* Stub implementation for convert_color */
void convert_color(void) {}



/* Stub implementation for convert_timestamp */
void convert_timestamp(void) {}



/* Stub implementation for convert_alignment */
void convert_alignment(void) {}
