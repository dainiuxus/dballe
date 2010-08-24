/*
 * DB-ALLe - Archive for punctual meteorological data
 *
 * Copyright (C) 2005--2010  ARPA-SIM <urpsim@smr.arpa.emr.it>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 * Author: Enrico Zini <enrico@enricozini.com>
 */

#include "config.h"

#include "opcode.h"
#include "msg.h"
#include <dballe/core/file.h>
#include <dballe/core/conv.h>

#include <stdio.h>
#include <netinet/in.h>

#include <stdlib.h>	/* malloc */
#include <ctype.h>	/* isspace */
#include <string.h>	/* memcpy */
#include <stdarg.h>	/* va_start, va_end */
#include <math.h>	/* NAN */
#include <time.h>
#include <errno.h>

#include <assert.h>

/* #define TRACE_DECODER */

#ifdef TRACE_DECODER
#define TRACE(...) fprintf(stderr, __VA_ARGS__)
#define IFTRACE if (1)
#else
#define TRACE(...) do { } while (0)
#define IFTRACE if (0)
#endif

extern "C" {
dba_err bufr_decoder_decode_header(dba_rawmsg in, bufrex_msg out);
dba_err bufr_decoder_decode(dba_rawmsg in, bufrex_msg out);
}

using namespace std;

// Unmarshal a big endian integer value n bytes long
static inline int readNumber(const unsigned char* buf, int bytes)
{
	int res = 0;
	int i;
	for (i = 0; i < bytes; ++i)
	{
		res <<= 8;
		res |= buf[i];
	}
	return res;
}

// Return a value with bitlen bits set to 1
static inline uint32_t all_ones(int bitlen)
{
	return ((1 << (bitlen - 1))-1) | (1 << (bitlen - 1));
}


namespace {

static const double e10[] = {
	1.0,
	10.0,
	100.0,
	1000.0,
	10000.0,
	100000.0,
	1000000.0,
	10000000.0,
	100000000.0,
	1000000000.0,
	10000000000.0,
	100000000000.0,
	1000000000000.0,
	10000000000000.0,
	100000000000000.0,
	1000000000000000.0,
	10000000000000000.0,
};

static double decode_int(uint32_t ival, dba_varinfo info)
{
	double val = ival;
	
	if (info->bufr_scale >= 0)
		val = (val + info->bit_ref) / e10[info->bufr_scale];
	else
		val = (val + info->bit_ref) * e10[-info->bufr_scale];
	//val = (val + info->bit_ref) / pow(10, info->scale);

	return val;
}

#define CHECK_AVAILABLE_DATA(start, datalen, next) do { \
	if (start + datalen > in->buf + in->len) \
		return parse_error(start, "end of BUFR message while looking for " next); \
	TRACE(next " starts at %d and contains at least %d bytes\n", (int)(start - in->buf), datalen); \
} while (0)



struct decoder 
{
	/* Input message data */
	dba_rawmsg in;
	/* Output decoded variables */
	bufrex_msg out;

	/* Offset of the start of BUFR section 1 */
	const unsigned char* sec1;
	/* Offset of the start of BUFR section 2 */
	const unsigned char* sec2;
	/* Offset of the start of BUFR section 3 */
	const unsigned char* sec3;
	/* Offset of the start of BUFR section 4 */
	const unsigned char* sec4;
	/* Offset of the start of BUFR section 5 */
	const unsigned char* sec5;

	decoder(dba_rawmsg in, bufrex_msg out)
		: in(in), out(out), sec1(0), sec2(0), sec3(0), sec4(0), sec5(0)
	{
	}

	~decoder()
	{
	}

	const char* filename() const
	{
		if (in->file == NULL)
			return "(memory)";
		else
			return dba_file_name(in->file);
	}

	dba_err parse_error(const unsigned char* pos, const char* fmt, ...)
	{
		char* context;
		char* message;

		va_list ap;
		va_start(ap, fmt);
		vasprintf(&message, fmt, ap);
		va_end(ap);

		asprintf(&context, "parsing %s:%d+%d", filename(), in->offset, pos - in->buf);
		
		return dba_error_generic0(DBA_ERR_PARSE, context, message);
	}

	dba_err decode_sec1ed3()
	{
		// TODO: misses master table number in sec1[3]
		// Update sequence number sec1[6]
		out->opt.bufr.update_sequence_number = sec1[6];
		// Set length to 1 for now, will set the proper length later when we
		// parse the section itself
		out->opt.bufr.optional_section_length = (sec1[7] & 0x80) ? 1 : 0;
		// subcentre in sec1[4]
		out->opt.bufr.subcentre = (int)sec1[4];
		// centre in sec1[5]
		out->opt.bufr.centre = (int)sec1[5];
		out->opt.bufr.master_table = sec1[10];
		out->opt.bufr.local_table = sec1[11];
		out->type = (int)sec1[8];
		out->subtype = 255;
		out->localsubtype = (int)sec1[9];

		out->rep_year = (int)sec1[12];
		// Fix the century with a bit of euristics
		if (out->rep_year > 50)
			out->rep_year += 1900;
		else
			out->rep_year += 2000;
		out->rep_month = (int)sec1[13];
		out->rep_day = (int)sec1[14];
		out->rep_hour = (int)sec1[15];
		out->rep_minute = (int)sec1[16];
		if ((int)sec1[17] != 0)
			out->rep_year = (int)sec1[17] * 100 + (out->rep_year % 100);
		return dba_error_ok();
	}

	dba_err decode_sec1ed4()
	{
		// TODO: misses master table number in sec1[3]
		// TODO: misses update sequence number sec1[8]
		// centre in sec1[4-5]
		out->opt.bufr.centre = readNumber(sec1+4, 2);
		// subcentre in sec1[6-7]
		out->opt.bufr.subcentre = readNumber(sec1+6, 2);
		// has_optional in sec1[9]
		// Set length to 1 for now, will set the proper length later when we
		// parse the section itself
		out->opt.bufr.optional_section_length = (sec1[9] & 0x80) ? 1 : 0;
		// category in sec1[10]
		out->type = (int)sec1[10];
		// international data sub-category in sec1[11]
		out->subtype = (int)sec1[11];
		// local data sub-category in sec1[12]
		out->localsubtype = (int)sec1[12];
		// version number of master table in sec1[13]
		out->opt.bufr.master_table = sec1[13];
		// version number of local table in sec1[14]
		out->opt.bufr.local_table = sec1[14];
		// year in sec1[15-16]
		out->rep_year = readNumber(sec1 + 15, 2);
		// month in sec1[17]
		out->rep_month = (int)sec1[17];
		// day in sec1[18]
		out->rep_day = (int)sec1[18];
		// hour in sec1[19]
		out->rep_hour = (int)sec1[19];
		// minute in sec1[20]
		out->rep_minute = (int)sec1[20];
		// sec in sec1[21]
		out->rep_second = (int)sec1[21];
		return dba_error_ok();
	}

	/* Decode the message header only */
	dba_err decode_header()
	{
		int i;

		/* Read BUFR section 0 (Indicator section) */
		CHECK_AVAILABLE_DATA(in->buf, 8, "section 0 of BUFR message (indicator section)");
		sec1 = in->buf + 8;
		if (memcmp(in->buf, "BUFR", 4) != 0)
			return parse_error(in->buf, "data does not start with BUFR header (\"%.4s\" was read instead)", in->buf);
		TRACE(" -> is BUFR\n");

		/* Check the BUFR edition number */
		if (in->buf[7] != 2 &&in->buf[7] != 3 && in->buf[7] != 4)
			return parse_error(in->buf + 7, "Only BUFR edition 3 and 4 are supported (this message is edition %d)", (unsigned)in->buf[7]);
		out->edition = in->buf[7];

		/* Read bufr section 1 (Identification section) */
		CHECK_AVAILABLE_DATA(sec1, 18, "section 1 of BUFR message (identification section)");
		sec2 = sec1 + readNumber(sec1, 3);
		if (sec2 > in->buf + in->len)
			return parse_error(sec1, "Section 1 claims to end past the end of the BUFR message");

		switch (out->edition)
		{
			case 2: DBA_RUN_OR_RETURN(decode_sec1ed3()); break;
			case 3: DBA_RUN_OR_RETURN(decode_sec1ed3()); break;
			case 4: DBA_RUN_OR_RETURN(decode_sec1ed4()); break;
			default:
				return dba_error_consistency("BUFR edition is %d, but I can only decode 3 and 4", out->edition);
		}

		TRACE(" -> opt %d upd %d origin %d.%d tables %d.%d type %d.%d %04d-%02d-%02d %02d:%02d\n", 
				out->opt.bufr.optional_section_length, (int)sec1[6],
				out->opt.bufr.centre, out->opt.bufr.subcentre,
				out->opt.bufr.master_table, out->opt.bufr.local_table,
				out->type, out->subtype,
				out->rep_year, out->rep_month, out->rep_day, out->rep_hour, out->rep_minute);

#if 0
		fprintf(stderr, "S1 Len %d  table #%d  osc %d  oc %d  seq #%d  optsec %x\n",
				bufr_sec1_len,
				master_table,
				(int)psec1[4],
				(int)psec1[5],
				(int)psec1[6],
				(int)psec1[7]);
			
		fprintf(stderr, "S1 cat %d  subc %d  verm %d verl %d  %d/%d/%d %d:%d\n",
				(int)psec1[8],
				(int)psec1[9],
				(int)psec1[10],
				(int)psec1[11],

				(int)psec1[12],
				(int)psec1[13],
				(int)psec1[14],
				(int)psec1[15],
				(int)psec1[16]);
#endif
		
		/* Read BUFR section 2 (Optional section) */
		if (out->opt.bufr.optional_section_length)
		{
			CHECK_AVAILABLE_DATA(sec2, 4, "section 2 of BUFR message (optional section)");
			sec3 = sec2 + readNumber(sec2, 3);
			out->opt.bufr.optional_section_length = readNumber(sec2, 3) - 4;
			out->opt.bufr.optional_section = (char*)calloc(1, out->opt.bufr.optional_section_length);
			if (out->opt.bufr.optional_section == NULL)
				return dba_error_alloc("allocating space for the optional section");
			memcpy(out->opt.bufr.optional_section, sec2 + 4, out->opt.bufr.optional_section_length);
			if (sec3 > in->buf + in->len)
				return parse_error(sec2, "Section 2 claims to end past the end of the BUFR message");
		} else
			sec3 = sec2;

		/* Read BUFR section 3 (Data description section) */
		CHECK_AVAILABLE_DATA(sec3, 8, "section 3 of BUFR message (data description section)");
		sec4 = sec3 + readNumber(sec3, 3);
		if (sec4 > in->buf + in->len)
			return parse_error(sec3, "Section 3 claims to end past the end of the BUFR message");
		out->opt.bufr.subsets = readNumber(sec3 + 4, 2);
		out->opt.bufr.compression = (sec3[6] & 0x40) ? 1 : 0;
		for (i = 0; i < (sec4 - sec3 - 7)/2; i++)
		{
			dba_varcode var = (dba_varcode)readNumber(sec3 + 7 + i * 2, 2);
			DBA_RUN_OR_RETURN(bufrex_msg_append_datadesc(out, var));
		}
		TRACE(" s3length %d subsets %d observed %d compression %d byte7 %x\n",
				(int)(sec4 - sec3), out->opt.bufr.subsets, (sec3[6] & 0x80) ? 1 : 0, out->opt.bufr.compression, (unsigned int)sec3[6]);
		/*
		IFTRACE{
			TRACE(" -> data descriptor section: ");
			bufrex_opcode_print(msg->datadesc, stderr);
			TRACE("\n");
		}
		*/

		return dba_error_ok();
	}

	/* Decode message data section after the header has been decoded */
	dba_err decode_data();
};

struct opcode_interpreter
{
	decoder& d;

	/* Current subset when decoding non-compressed BUFR messages */
	bufrex_subset current_subset;

	/* Current value of scale change from C modifier */
	int c_scale_change;
	/* Current value of width change from C modifier */
	int c_width_change;

	/* List of opcodes to decode */
	bufrex_opcode ops;

	/* Bit decoding data */
	int cursor;
	unsigned char pbyte;
	int pbyte_len;

	/* Data present bitmap */
	dba_var bitmap;
	/* Number of elements set to true in the bitmap */
	int bitmap_count;
	/* Next bitmap element for which we decode values */
	int bitmap_use_index;
	/* Next subset element for which we decode attributes */
	int bitmap_subset_index;

	opcode_interpreter(decoder& d, int start_ofs)
		: d(d), current_subset(0),
		  c_scale_change(0), c_width_change(0),
		  ops(0), cursor(start_ofs), pbyte(0), pbyte_len(0),
		  bitmap(0), bitmap_count(0)
	{
	}

	~opcode_interpreter()
	{
		if (ops != NULL)
			bufrex_opcode_delete(&ops);
	}

	dba_err parse_error(const char* fmt, ...)
	{
		char* context;
		char* message;

		va_list ap;
		va_start(ap, fmt);
		vasprintf(&message, fmt, ap);
		va_end(ap);

		asprintf(&context, "parsing %s:%d+%d", filename(), d.in->offset, cursor);
		
		return dba_error_generic0(DBA_ERR_PARSE, context, message);
	}

	/// Return the currently decoded filename
	const char* filename() const { return d.filename(); }

	/* Return the current decoding byte offset */
	int offset() const { return cursor; }

	/* Return the number of bits left in the message to be decoded */
	int bits_left() const { return (d.in->len - cursor) * 8 + pbyte_len; }

	/**
	 * Get the integer value of the next 'n' bits from the decode input
	 * n must be <= 32.
	 */
	dba_err get_bits(int n, uint32_t *val)
	{
		uint32_t result = 0;
		int i;

		if (cursor == d.in->len)
			return parse_error("end of buffer while looking for %d bits of bit-packed data", n);

		for (i = 0; i < n; i++) 
		{
			if (pbyte_len == 0) 
			{
				pbyte_len = 8;
				pbyte = d.in->buf[cursor++];
			}
			result <<= 1;
			if (pbyte & 0x80)
				result |= 1;
			pbyte <<= 1;
			pbyte_len--;
		}

		*val = result;

		return dba_error_ok();
	}

	/* Dump 'count' bits of 'buf', starting at the 'ofs-th' bit */
	void dump_next_bits(int count, FILE* out)
	{
		int cursor = cursor;
		int pbyte = pbyte;
		int pbyte_len = pbyte_len;
		int i;

		for (i = 0; i < count; ++i) 
		{
			if (cursor == d.in->len)
				break;
			if (pbyte_len == 0) 
			{
				pbyte_len = 8;
				pbyte = d.in->buf[cursor++];
				putc(' ', out);
			}
			putc((pbyte & 0x80) ? '1' : '0', out);
			pbyte <<= 1;
			--pbyte_len;
		}
	}

	dba_err bitmap_next()
	{
		TRACE("bitmap_next pre %d %d %zd\n", bitmap_use_index, bitmap_subset_index, bitmap.size());
		if (bitmap == 0)
			return parse_error("applying a data present bitmap with no current bitmap");
		if (d.out->subsets_count == 0)
			return parse_error("no subsets created yet, but already applying a data present bitmap");
		++bitmap_use_index;
		++bitmap_subset_index;
		while (bitmap_use_index < dba_var_info(bitmap)->len &&
			(bitmap_use_index < 0 || dba_var_value(bitmap)[bitmap_use_index] == '-'))
		{
			TRACE("INCR\n");
			++bitmap_use_index;
			++bitmap_subset_index;
			while (bitmap_subset_index < d.out->subsets[0]->vars_count &&
				DBA_VAR_F(dba_var_code(d.out->subsets[0]->vars[bitmap_subset_index])) != 0)
				++bitmap_subset_index;
		}
		if (bitmap_use_index == dba_var_info(bitmap)->len)
			return parse_error("end of data present bitmap reached");
		if (bitmap_subset_index == d.out->subsets[0]->vars_count)
			return parse_error("end of data reached when applying attributes");
		TRACE("bitmap_next post %d %d\n", bitmap_use_index, bitmap_subset_index);
		return dba_error_ok();
	}

	dba_err decode_b_data();
	dba_err decode_bitmap(dba_varcode code);
	virtual dba_err decode_b_string(dba_varinfo info);
	virtual dba_err decode_b_num(dba_varinfo info);

	/**
	 * Decode instant or delayed replication information.
	 *
	 * In case of delayed replication, store a variable in the subset(s)
	 * with the repetition count.
	 */
	dba_err decode_replication_info(int& group, int& count);
	dba_err decode_r_data();
	dba_err decode_c_data();

	dba_err add_var(bufrex_subset subset, dba_var& var)
	{
		if (bitmap && DBA_VAR_X(dba_var_code(var)) == 33)
		{
			TRACE("Adding var %01d%02d%03d %s as attribute to %01d%02d%03d bsi %d/%d\n",
					DBA_VAR_F(dba_var_code(var)),
					DBA_VAR_X(dba_var_code(var)),
					DBA_VAR_Y(dba_var_code(var)),
					dba_var_value(var),
					DBA_VAR_F(dba_var_code(subset->vars[bitmap_subset_index])),
					DBA_VAR_X(dba_var_code(subset->vars[bitmap_subset_index])),
					DBA_VAR_Y(dba_var_code(subset->vars[bitmap_subset_index])),
					bitmap_subset_index, subset->vars_count);
			DBA_RUN_OR_RETURN(dba_var_seta_nocopy(subset->vars[bitmap_subset_index], var));
		}
		else
		{
			TRACE("Adding var %01d%02d%03d %s to subset\n",
					DBA_VAR_F(dba_var_code(var)),
					DBA_VAR_X(dba_var_code(var)),
					DBA_VAR_Y(dba_var_code(var)),
					dba_var_value(var));
			DBA_RUN_OR_RETURN(bufrex_subset_store_variable(subset, var));
		}
		var = NULL;
		return dba_error_ok();
	}

	/* Run the opcode interpreter to decode the data section */
	dba_err decode_data_section()
	{
		dba_err err = DBA_OK;

		/*
		fprintf(stderr, "read_data: ");
		bufrex_opcode_print(ops, stderr);
		fprintf(stderr, "\n");
		*/
		TRACE("bufr_message_decode_data_section: START\n");

		while (ops != NULL)
		{
			IFTRACE{
				TRACE("bufr_message_decode_data_section TODO: ");
				bufrex_opcode_print(ops, stderr);
				TRACE("\n");
			}

			switch (DBA_VAR_F(ops->val))
			{
				case 0:
					DBA_RUN_OR_RETURN(decode_b_data());
					break;
				case 1:
					DBA_RUN_OR_RETURN(decode_r_data());
					break;
				case 2:
					DBA_RUN_OR_RETURN(decode_c_data());
					/* DBA_RUN_OR_RETURN(bufr_message_decode_c_data(msg, ops, &s)); */
					break;
				case 3:
				{
					/* D table opcode: expand the chain */
					bufrex_opcode op;
					bufrex_opcode exp;

					/* Pop the first opcode */
					DBA_RUN_OR_RETURN(bufrex_opcode_pop(&(ops), &op));
					
					if ((err = bufrex_msg_query_dtable(d.out, op->val, &exp)) != DBA_OK)
					{
						bufrex_opcode_delete(&op);
						return err;
					}
					bufrex_opcode_delete(&op);

					/* Push the expansion back in the fields */
					if ((err = bufrex_opcode_join(&exp, ops)) != DBA_OK)
					{
						bufrex_opcode_delete(&exp);
						return err;
					}
					ops = exp;
					break;
				}
				default:
					return parse_error("cannot handle field %01d%02d%03d",
								DBA_VAR_F(ops->val),
								DBA_VAR_X(ops->val),
								DBA_VAR_Y(ops->val));
			}
		}

		return dba_error_ok();
	}

	dba_err run()
	{
		int i;

		if (d.out->opt.bufr.compression)
		{
			/* Only needs to parse once */
			current_subset = NULL;
			DBA_RUN_OR_RETURN(bufrex_msg_get_datadesc(d.out, &(ops)));
			DBA_RUN_OR_RETURN(decode_data_section());
		} else {
			/* Iterate on the number of subgroups */
			for (i = 0; i < d.out->opt.bufr.subsets; ++i)
			{
				DBA_RUN_OR_RETURN(bufrex_msg_get_subset(d.out, i, &(current_subset)));
				DBA_RUN_OR_RETURN(bufrex_msg_get_datadesc(d.out, &(ops)));
				DBA_RUN_OR_RETURN(decode_data_section());
			}
		}

		IFTRACE {
		if (bits_left() > 32)
		{
			fprintf(stderr, "The data section of %s still contains %d unparsed bits\n",
					filename(), bits_left() - 32);
			/*
			err = dba_error_parse(msg->file->name, POS + vec->cursor,
					"the data section still contains %d unparsed bits",
					bitvec_bits_left(vec));
			goto fail;
			*/
		}
		}
	}
};

struct opcode_interpreter_compressed : public opcode_interpreter
{
	opcode_interpreter_compressed(decoder& d, int start_ofs)
		: opcode_interpreter(d, start_ofs) {}
	virtual dba_err decode_b_num(dba_varinfo info);
};

dba_err decoder::decode_data()
{
	int i;

	/* Load decoding tables */
	DBA_RUN_OR_RETURN(bufrex_msg_load_tables(out));

	/* Read BUFR section 4 (Data section) */
	CHECK_AVAILABLE_DATA(sec4, 4, "section 4 of BUFR message (data section)");

	sec5 = sec4 + readNumber(sec4, 3);
	if (sec5 > in->buf + in->len)
		return parse_error(sec4, "Section 4 claims to end past the end of the BUFR message");
	TRACE("section 4 is %d bytes long (%02x %02x %02x %02x)\n", readNumber(sec4, 3),
			(unsigned int)*(sec4), (unsigned int)*(sec4+1), (unsigned int)*(sec4+2), (unsigned int)*(sec4+3));

	if (out->opt.bufr.compression)
	{
		opcode_interpreter_compressed interpreter(*this, sec4 + 4 - in->buf);
		interpreter.run();
	} else {
		opcode_interpreter interpreter(*this, sec4 + 4 - in->buf);
		interpreter.run();
	}

	/* Read BUFR section 5 (Data section) */
	CHECK_AVAILABLE_DATA(sec5, 4, "section 5 of BUFR message (end section)");

	if (memcmp(sec5, "7777", 4) != 0)
		return parse_error(sec5, "section 5 does not contain '7777'");

#if 0
	for (i = 0; i < out->opt.bufr.subsets; ++i)
	{
		bufrex_subset subset;
		DBA_RUN_OR_RETURN(bufrex_msg_get_subset(out, i, &subset));
		/* Copy the decoded attributes into the decoded variables */
		DBA_RUN_OR_RETURN(bufrex_subset_apply_attributes(subset));
	}
#endif

	return dba_error_ok();
}

}



dba_err bufr_decoder_decode_header(dba_rawmsg in, bufrex_msg out)
{
	dba_err err;

	bufrex_msg_reset(out);

	decoder d(in, out);

	DBA_RUN_OR_GOTO(fail, d.decode_header());

	return dba_error_ok();

fail:
	TRACE(" -> bufr_message_decode_header failed.\n");
	return err;
}

dba_err bufr_decoder_decode(dba_rawmsg in, bufrex_msg out)
{
	dba_err err;

	bufrex_msg_reset(out);

	decoder d(in, out);

	DBA_RUN_OR_GOTO(fail, d.decode_header());
	DBA_RUN_OR_GOTO(fail, d.decode_data());

	return dba_error_ok();

fail:
	TRACE(" -> bufr_message_decode failed.\n");
	return err;
}


dba_err opcode_interpreter::decode_b_data()
{
	dba_err err = DBA_OK;
	dba_varinfo info = NULL;

	IFTRACE {
		TRACE("bufr_message_decode_b_data: items: ");
		bufrex_opcode_print(ops, stderr);
		TRACE("\n");
	}

	DBA_RUN_OR_RETURN(dba_vartable_query_altered(
				d.out->btable, ops->val,
				DBA_ALT(c_width_change, c_scale_change), &info));

	IFTRACE {
		TRACE("Parsing @%d+%d [bl %d+%d sc %d+%d ref %d]: %d%02d%03d %s[%s]\n", cursor, 8-pbyte_len,
				info->bit_len, c_width_change,
				info->scale, c_scale_change,
				info->bit_ref,
				DBA_VAR_F(info->var), DBA_VAR_X(info->var), DBA_VAR_Y(info->var),
				info->desc, info->unit);
		dump_next_bits(64, stderr);
		TRACE("\n");
	}

	if (c_scale_change > 0)
		TRACE("Applied %d scale change\n", c_scale_change);
	if (c_width_change > 0)
		TRACE("Applied %d width change\n", c_width_change);

	/* Get the real datum */
	if (VARINFO_IS_STRING(info))
	{
		DBA_RUN_OR_GOTO(cleanup, decode_b_string(info));
	} else {
		DBA_RUN_OR_GOTO(cleanup, decode_b_num(info));
	}

	/* Remove from the chain the item that we handled */
	{
		bufrex_opcode op;
		DBA_RUN_OR_RETURN(bufrex_opcode_pop(&(ops), &op));
		bufrex_opcode_delete(&op);
	}

	/*
	IFTRACE {
		TRACE("bufr_message_decode_b_data (items:");
		bufrex_opcode_print(*ops, stderr);
		TRACE(")\n");
	}
	*/

cleanup:
	return err == DBA_OK ? dba_error_ok() : err;
}

dba_err opcode_interpreter::decode_b_string(dba_varinfo info)
{
	/* Read a string */
	dba_err err = DBA_OK;
	dba_var var = NULL;
	char *str = NULL;
	int toread = info->bit_len;
	int len = 0;
	int missing = 1;

	str = (char*)malloc(info->bit_len / 8 + 2);
	if (str == NULL)
	{
		err = dba_error_alloc("allocating space for a BUFR string variable");
		goto cleanup;
	}

	while (toread > 0)
	{
		uint32_t bitval;
		int count = toread > 8 ? 8 : toread;
		DBA_RUN_OR_GOTO(cleanup, get_bits(count, &bitval));
		/* Check that the string is not all 0xff, meaning missing value */
		if (bitval != 0xff && bitval != 0)
			missing = 0;
		str[len++] = bitval;
		toread -= count;
	}

	if (!missing)
	{
		str[len] = 0;

		/* Convert space-padding into zero-padding */
		for (--len; len > 0 && isspace(str[len]);
				len--)
			str[len] = 0;
	}

	TRACE("bufr_message_decode_b_data len %d val %s missing %d info-len %d info-desc %s\n", len, str, missing, info->bit_len, info->desc);

	if (DBA_VAR_X(info->var) == 33)
		DBA_RUN_OR_GOTO(cleanup, bitmap_next());

	/* Store the variable that we found */
	if (d.out->opt.bufr.compression)
	{
		uint32_t diffbits;
		int i;
		/* If compression is in use, then we just decoded the base value.  Now
		 * we need to decode all the offsets */

		/* Decode the number of bits (encoded in 6 bits) that these difference
		 * values occupy */
		DBA_RUN_OR_GOTO(cleanup, get_bits(6, &diffbits));

		TRACE("Compressed string, diff bits %d\n", diffbits);

		if (diffbits != 0)
		{
			/* For compressed strings, the reference value must be all zeros */
			for (i = 0; i < len; ++i)
				if (str[i] != 0)
				{
					err = dba_error_unimplemented("compressed strings with %d bit deltas have non-zero reference value", diffbits);
					goto cleanup;
				}

			/* Let's also check that the number of
			 * difference characters is the same length as
			 * the reference string */
			if (diffbits > len)
			{
				err = dba_error_unimplemented("compressed strings with %d characters have %d bit deltas (deltas should not be longer than field)", len, diffbits);
				goto cleanup;
			}

			for (i = 0; i < d.out->opt.bufr.subsets; ++i)
			{
				int j, missing = 1;

				/* Access the subset we are working on */
				bufrex_subset subset;
				DBA_RUN_OR_GOTO(cleanup, bufrex_msg_get_subset(d.out, i, &subset));

				/* Decode the difference value, reusing the str buffer */
				for (j = 0; j < diffbits; ++j)
				{
					uint32_t bitval;
					DBA_RUN_OR_GOTO(cleanup, get_bits(8, &bitval));
					/* Check that the string is not all 0xff, meaning missing value */
					if (bitval != 0xff && bitval != 0)
						missing = 0;
					str[j] = bitval;
				}
				str[j] = 0;

				if (missing)
				{
					/* Missing value */
					TRACE("Decoded[%d] as missing\n", i);

					/* Create the new dba_var */
					DBA_RUN_OR_GOTO(cleanup, dba_var_create(info, &var));
				} else {
					/* Compute the value for this subset */
					TRACE("Decoded[%d] as %s\n", i, str);

					DBA_RUN_OR_GOTO(cleanup, dba_var_createc(info, str, &var));
				}

				/* Add it to this subset */
				DBA_RUN_OR_GOTO(cleanup, add_var(subset, var));
			}
		} else {
			/* Add the string to all the subsets */
			for (i = 0; i < d.out->opt.bufr.subsets; ++i)
			{
				bufrex_subset subset;
				DBA_RUN_OR_GOTO(cleanup, bufrex_msg_get_subset(d.out, i, &subset));

				/* Create the new dba_var */
				if (missing)
					DBA_RUN_OR_GOTO(cleanup, dba_var_create(info, &var));
				else
					DBA_RUN_OR_GOTO(cleanup, dba_var_createc(info, str, &var));

				DBA_RUN_OR_GOTO(cleanup, add_var(subset, var));
			}
		}
	} else {
		/* Create the new dba_var */
		if (missing)
			DBA_RUN_OR_GOTO(cleanup, dba_var_create(info, &var));
		else
			DBA_RUN_OR_GOTO(cleanup, dba_var_createc(info, str, &var));
		DBA_RUN_OR_GOTO(cleanup, add_var(current_subset, var));
	}

cleanup:
	if (str != NULL)
		free(str);
	if (var != NULL)
		dba_var_delete(var);
	return err == DBA_OK ? dba_error_ok() : err;
}

dba_err opcode_interpreter::decode_b_num(dba_varinfo info)
{
	dba_err err = DBA_OK;
	dba_var var = NULL;
	/* Read a value */
	uint32_t val;
	int missing;

	if (DBA_VAR_X(info->var) == 33)
		DBA_RUN_OR_GOTO(cleanup, bitmap_next());

	DBA_RUN_OR_GOTO(cleanup, get_bits(info->bit_len, &val));

	TRACE("Reading %s (%s), size %d, scale %d, starting point %d\n", info->desc, info->bufr_unit, info->bit_len, info->scale, val);

	/* Check if there are bits which are not 1 (that is, if the value is present) */
	missing = (val == all_ones(info->bit_len));

	/*bufr_decoder_debug(decoder, "  %s: %d%s\n", info.desc, val, info.type);*/
	TRACE("bufr_message_decode_b_data len %d val %d info-len %d info-desc %s\n", info->bit_len, val, info->bit_len, info->desc);

	/* Store the variable that we found */
	if (missing)
	{
		/* Create the new dba_var */
		TRACE("Decoded as missing\n");
		DBA_RUN_OR_GOTO(cleanup, dba_var_create(info, &var));
	} else {
		double dval = decode_int(val, info);
		TRACE("Decoded as %f %s\n", dval, info->bufr_unit);
		/* Convert to target unit */
		DBA_RUN_OR_GOTO(cleanup, dba_convert_units(info->bufr_unit, info->unit, dval, &dval));
		TRACE("Converted to %f %s\n", dval, info->unit);
		/* Create the new dba_var */
		DBA_RUN_OR_GOTO(cleanup, dba_var_created(info, dval, &var));
	}
	DBA_RUN_OR_GOTO(cleanup, add_var(current_subset, var));

cleanup:
	if (var != NULL)
		dba_var_delete(var);
	return err == DBA_OK ? dba_error_ok() : err;
}

dba_err opcode_interpreter_compressed::decode_b_num(dba_varinfo info)
{
	dba_err err = DBA_OK;
	dba_var var = NULL;
	/* Read a value */
	uint32_t val;
	int missing;

	if (DBA_VAR_X(info->var) == 33)
		DBA_RUN_OR_GOTO(cleanup, bitmap_next());

	DBA_RUN_OR_GOTO(cleanup, get_bits(info->bit_len, &val));

	TRACE("Reading %s (%s), size %d, scale %d, starting point %d\n", info->desc, info->bufr_unit, info->bit_len, info->scale, val);

	/* Check if there are bits which are not 1 (that is, if the value is present) */
	missing = (val == all_ones(info->bit_len));

	/*bufr_decoder_debug(decoder, "  %s: %d%s\n", info.desc, val, info.type);*/
	TRACE("bufr_message_decode_b_data len %d val %d info-len %d info-desc %s\n", info->bit_len, val, info->bit_len, info->desc);

	/* Store the variable that we found */
	uint32_t diffbits;
	int i;
	/* If compression is in use, then we just decoded the base value.  Now
	 * we need to decode all the offsets */

	/* Decode the number of bits (encoded in 6 bits) that these difference
	 * values occupy */
	DBA_RUN_OR_GOTO(cleanup, get_bits(6, &diffbits));
	if (missing && diffbits != 0)
	{
		err = dba_error_consistency("When decoding compressed BUFR data, the difference bit length must be 0 (and not %d like in this case) when the base value is missing", diffbits);
		goto cleanup;
	}

	TRACE("Compressed number, base value %d diff bits %d\n", val, diffbits);

	for (i = 0; i < d.out->opt.bufr.subsets; ++i)
	{
		uint32_t diff, newval;
		/* Access the subset we are working on */
		bufrex_subset subset;
		DBA_RUN_OR_GOTO(cleanup, bufrex_msg_get_subset(d.out, i, &subset));

		/* Decode the difference value */
		DBA_RUN_OR_GOTO(cleanup, get_bits(diffbits, &diff));

		/* Check if it's all 1: in that case it's a missing value */
		if (missing || diff == all_ones(diffbits))
		{
			/* Missing value */
			TRACE("Decoded[%d] as missing\n", i);

			/* Create the new dba_var */
			DBA_RUN_OR_GOTO(cleanup, dba_var_create(info, &var));
		} else {
			/* Compute the value for this subset */
			newval = val + diff;
			double dval = decode_int(newval, info);
			TRACE("Decoded[%d] as %d+%d=%%f %s\n", i, val, diff, newval, dval, info->bufr_unit);

			/* Convert to target unit */
			DBA_RUN_OR_GOTO(cleanup, dba_convert_units(info->bufr_unit, info->unit, dval, &dval));
			TRACE("Converted to %f %s\n", dval, info->unit);

			/* Create the new dba_var */
			DBA_RUN_OR_GOTO(cleanup, dba_var_created(info, dval, &var));
		}

		/* Add it to this subset */
		DBA_RUN_OR_GOTO(cleanup, add_var(subset, var));
	}

cleanup:
	if (var != NULL)
		dba_var_delete(var);
	return err == DBA_OK ? dba_error_ok() : err;
}

dba_err opcode_interpreter::decode_replication_info(int& group, int& count)
{
	group = DBA_VAR_X(ops->val);
	count = DBA_VAR_Y(ops->val);

	/* Pop the R repetition node, since we have read its value in group and count */
	{
		bufrex_opcode op;
		DBA_RUN_OR_RETURN(bufrex_opcode_pop(&(ops), &op));
		bufrex_opcode_delete(&op);
	}

	if (count == 0)
	{
		/* Delayed replication */

		bufrex_opcode info_op;
		dba_varinfo info;
		dba_var rep_var;
		uint32_t _count;
		
		/* Read the descriptor for the delayed replicator count */
		DBA_RUN_OR_RETURN(bufrex_opcode_pop(&(ops), &info_op));

		/* Read variable informations about the delayed replicator count */
		DBA_RUN_OR_RETURN(bufrex_msg_query_btable(d.out, info_op->val, &info));
		
		/* Fetch the repetition count */
		DBA_RUN_OR_RETURN(get_bits(info->bit_len, &_count));
		count = _count;

		/* Insert the repetition count among the parsed variables */
		if (d.out->opt.bufr.compression)
		{
			uint32_t diffbits;
			uint32_t repval = 0;
			int i;
			/* If compression is in use, then we just decoded the base value.  Now
			 * we need to decode all the repetition factors and see
			 * that they are the same */

			/* Decode the number of bits (encoded in 6 bits) that these difference
			 * values occupy */
			DBA_RUN_OR_RETURN(get_bits(6, &diffbits));

			TRACE("Compressed delayed repetition, base value %d diff bits %d\n", count, diffbits);

			for (i = 0; i < d.out->opt.bufr.subsets; ++i)
			{
				uint32_t diff, newval;

				/* Decode the difference value */
				DBA_RUN_OR_RETURN(get_bits(diffbits, &diff));

				/* Compute the value for this subset */
				newval = count + diff;
				TRACE("Decoded[%d] as %d+%d=%d\n", i, count, diff, newval);

				if (i == 0)
					repval = newval;
				else if (repval != newval)
					return parse_error("compressed delayed replication factor has different values for subsets (%d and %d)", repval, newval);

				/* Access the subset we are working on */
				bufrex_subset subset;
				DBA_RUN_OR_RETURN(bufrex_msg_get_subset(d.out, i, &subset));

				/* Create the new dba_var */
				DBA_RUN_OR_RETURN(dba_var_createi(info, newval, &rep_var));

				/* Add it to this subset */
				DBA_RUN_OR_RETURN(bufrex_subset_store_variable(subset, rep_var));
			}
		} else {
			DBA_RUN_OR_RETURN(dba_var_createi(info, count, &rep_var));
			DBA_RUN_OR_RETURN(bufrex_subset_store_variable(current_subset, rep_var));
		}

		bufrex_opcode_delete(&info_op);
	/*	dba_var_delete(rep_var);   rep_var is taken in charge by bufrex_msg */

		TRACE("decode_replication_info %d items %d times (delayed)\n", group, count);
	} else
		TRACE("decode_replication_info %d items %d times\n", group, count);

	return dba_error_ok();
}

dba_err opcode_interpreter::decode_bitmap(dba_varcode code)
{
	int group;
	int count;

	bitmap = 0;

	DBA_RUN_OR_RETURN(decode_replication_info(group, count));

	// Sanity checks

	if (group != 1)
		return parse_error("bitmap section replicates %d descriptors instead of one", group);

	if (ops == NULL)
		return parse_error("there are no descriptor after bitmap replicator (expected B31031)");

	if (ops->val != DBA_VAR(0, 31, 31))
		return parse_error("bitmap element descriptor is %02d%02d%03d instead of B31031",
				DBA_VAR_F(ops->val), DBA_VAR_X(ops->val), DBA_VAR_Y(ops->val));

	// If compressed, ensure that the difference bits are 0 and they are
	// not trying to transmit odd things like delta bitmaps 
	if (d.out->opt.bufr.compression)
	{
		uint32_t diffbits;
		/* Decode the number of bits (encoded in 6 bits) that these difference
		 * values occupy */
		DBA_RUN_OR_RETURN(get_bits(6, &diffbits));
		if (diffbits != 0)
			return parse_error("bitmap declares %d difference bits per bitmap value, but we only support 0");
	}

	// Consume the data present indicator from the opcodes to process
	{
		bufrex_opcode op;
		DBA_RUN_OR_RETURN(bufrex_opcode_pop(&(ops), &op));
		bufrex_opcode_delete(&op);
	}

	// Bitmap size is now in count

	// Read the bitmap
	bitmap_count = 0;
	char bitmapstr[count + 1];
	for (int i = 0; i < count; ++i)
	{
		uint32_t val;
		DBA_RUN_OR_RETURN(get_bits(1, &val));
		bitmapstr[i] = (val == 0) ? '+' : '-';
		if (val == 0) ++bitmap_count;
	}
	bitmapstr[count] = 0;

	// Create a single use varinfo to store the bitmap
	dba_varinfo info;
	DBA_RUN_OR_RETURN(dba_varinfo_create_singleuse(code, &info));
	strcpy(info->desc, "DATA PRESENT BITMAP");
	strcpy(info->unit, "CCITTIA5");
	strcpy(info->bufr_unit, "CCITTIA5");
	info->len = count;
	info->bit_len = info->len * 8;

	// Store the bitmap
	// FIXME: leaks memory in case of errors, use the cleanup strategy
	DBA_RUN_OR_RETURN(dba_var_createc(info, bitmapstr, &bitmap));

	// Add var to subset(s)
	if (d.out->opt.bufr.compression)
	{
		for (int i = 0; i < d.out->opt.bufr.subsets; ++i)
		{
			bufrex_subset subset;
			DBA_RUN_OR_RETURN(bufrex_msg_get_subset(d.out, i, &subset));

			dba_var var1;
			if (i > 0)
				DBA_RUN_OR_RETURN(dba_var_copy(bitmap, &var1));
			else
				var1 = bitmap;

			DBA_RUN_OR_RETURN(add_var(subset, var1));
		}
	} else {
		DBA_RUN_OR_RETURN(add_var(current_subset, bitmap));
	}

	// Bitmap will stay set as a reference to the variable to use as the
	// current bitmap. The subset(s) are taking care of memory managing it.

	IFTRACE {
		TRACE("Decoded bitmap count %d: ", bitmap_count);
		for (size_t i = 0; i < dba_var_info(bitmap)->len; ++i)
			TRACE(dba_var_value(bitmap)[i]);
		TRACE("\n");
	}

	return dba_error_ok();
}

dba_err opcode_interpreter::decode_r_data()
{
	dba_err err = DBA_OK;
	int group, count;
	bufrex_opcode rep_op = NULL;
	
	TRACE("R DATA %01d%02d%03d %d %d\n", 
			DBA_VAR_F(ops->val), DBA_VAR_X(ops->val), DBA_VAR_Y(ops->val), group, count);

	/* Read replication information */
	DBA_RUN_OR_RETURN(decode_replication_info(group, count));

	/* Pop the first `group' nodes, since we handle them here */
	DBA_RUN_OR_RETURN(bufrex_opcode_pop_n(&(ops), &rep_op, group));

	/* Perform replication */
	while (count--)
	{
		bufrex_opcode saved = NULL;
		bufrex_opcode chain = NULL;

		/* fprintf(stderr, "Debug: rep %d left\n", count); */
		DBA_RUN_OR_GOTO(cleanup, bufrex_opcode_prepend(&chain, rep_op));

		IFTRACE{
			TRACE("bufr_decode_r_data %d items %d times left; items: ", group, count);
			bufrex_opcode_print(chain, stderr);
			TRACE("\n");
		}

		saved = ops;
		ops = chain;
		if ((err = decode_data_section()) != DBA_OK)
		{
			if (ops != NULL)
				bufrex_opcode_delete(&(ops));
			ops = saved;
			goto cleanup;
		}
		/* chain should always be NULL when parsing succeeds */
		assert(ops == NULL);
		ops = saved;
	}

cleanup:
	if (rep_op != NULL)
		bufrex_opcode_delete(&rep_op);

	return err == DBA_OK ? dba_error_ok() : err;
}

dba_err opcode_interpreter::decode_c_data()
{
	dba_err err = DBA_OK;
	dba_varcode code = ops->val;

	TRACE("C DATA %01d%02d%03d\n", DBA_VAR_F(code), DBA_VAR_X(code), DBA_VAR_Y(code));

	/* Pop the C node, since we have read its value in `code' */
	{
		bufrex_opcode op;
		DBA_RUN_OR_RETURN(bufrex_opcode_pop(&(ops), &op));
		bufrex_opcode_delete(&op);
	}

	switch (DBA_VAR_X(code))
	{
		case 1:
			c_width_change = DBA_VAR_Y(code) == 0 ? 0 : DBA_VAR_Y(code) - 128;
			break;
		case 2:
			c_scale_change = DBA_VAR_Y(code) == 0 ? 0 : DBA_VAR_Y(code) - 128;
			break;
		case 5: {
			int cdatalen = DBA_VAR_Y(code);
			char buf[300];
			int i;
			TRACE("C DATA character data %d long\n", cdatalen);
			for (i = 0; i < cdatalen; ++i)
			{
				uint32_t bitval;
				DBA_RUN_OR_GOTO(cleanup, get_bits(8, &bitval));
				TRACE("C DATA decoded character %d %c\n", (int)bitval, (char)bitval);
				buf[i] = bitval;
			}
			buf[i] = 0;
			TRACE("C DATA decoded string %s\n", buf);
			break;
		}
		case 22:
			if (DBA_VAR_Y(code) == 0)
			{
				DBA_RUN_OR_GOTO(cleanup, decode_bitmap(code));
				// Move to first bitmap use index
				bitmap_use_index = -1;
				bitmap_subset_index = -1;
			} else {
				err = parse_error("C modifier %d%02d%03d not yet supported",
							DBA_VAR_F(code),
							DBA_VAR_X(code),
							DBA_VAR_Y(code));
				goto cleanup;
			}
			break;
		case 24:
			if (DBA_VAR_Y(code) == 0)
			{
				DBA_RUN_OR_GOTO(cleanup, decode_r_data());
			} else {
				err = parse_error("C modifier %d%02d%03d not yet supported",
							DBA_VAR_F(code),
							DBA_VAR_X(code),
							DBA_VAR_Y(code));
				goto cleanup;
			}
			break;
		default:
			err = parse_error("C modifiers (%d%02d%03d in this case) are not yet supported",
						DBA_VAR_F(code),
						DBA_VAR_X(code),
						DBA_VAR_Y(code));
			goto cleanup;
	}

cleanup:
	return err == DBA_OK ? dba_error_ok() : err;
}
/* vim:set ts=4 sw=4: */
