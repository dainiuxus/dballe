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

#include <test-utils-bufrex.h>
#include <string.h>

namespace tut {
using namespace tut_dballe;

struct bufr_encoder_shar
{
	TestBufrexEnv testenv;

	bufr_encoder_shar()
	{
	}

	~bufr_encoder_shar()
	{
	}
};
TESTGRP(bufr_encoder);

bool memfind(dba_rawmsg rmsg, const char* str, size_t len)
{
	for (size_t i = 0; true; ++i)
	{
		if (i + len >= rmsg->len) return false;
		if (memcmp(rmsg->buf + i, str, len) == 0)
			return true;
	}
}

template<> template<>
void to::test<1>()
{
	bufrex_msg msg;

	CHECKED(bufrex_msg_create(BUFREX_BUFR, &msg));

	/* Initialise common message bits */
	msg->edition = 3;            // BUFR ed.4
	msg->type = 0;               // Template 8.255.171
	msg->subtype = 255;
	msg->localsubtype = 0;
	msg->opt.bufr.centre = 98;
	msg->opt.bufr.subcentre = 0;
	msg->opt.bufr.master_table = 12;
	msg->opt.bufr.local_table = 1;
	msg->opt.bufr.compression = 1;
	msg->rep_year = 2008;
	msg->rep_month = 5;
	msg->rep_day = 3;
	msg->rep_hour = 12;
	msg->rep_minute = 30;
	msg->rep_second = 0;

	/* Load encoding tables */
	CHECKED(bufrex_msg_load_tables(msg));

	/* Fill up the data descriptor section */
	CHECKED(bufrex_msg_append_datadesc(msg, DBA_VAR(0,  0,  13)));

	/* Get the working subset */
	bufrex_subset s;
	CHECKED(bufrex_msg_get_subset(msg, 0, &s));

	/* Set a text variable */
	CHECKED(bufrex_subset_store_variable_c(s, DBA_VAR(0, 0, 13), "12345678901234567890"));

	/* Set it to a shorter text, to see if the encoder encodes the trailing garbage */
	CHECKED(dba_var_setc(s->vars[0], "abcdefg"));

	/* Encode */
	dba_rawmsg rmsg = NULL;
	CHECKED(bufrex_msg_encode(msg, &rmsg));

	// Ensure that the encoded strings are space-padded
	gen_ensure(memfind(rmsg, "abcdefg       ", 14));

	// Decode the message
	bufrex_msg msg1;
	CHECKED(bufrex_msg_create(BUFREX_BUFR, &msg1));
	CHECKED(bufrex_msg_decode(msg1, rmsg));

	dba_rawmsg_delete(rmsg);

	TestBufrexMsg test;
	test.edition = 3;
	test.cat = 0;
	test.subcat = 255;
	test.localsubcat = 0;
	test.subsets = 1;
	test.subset(0).vars = 1;
	test.subset(0).set(DBA_VAR(0, 0, 13), "abcdefg");
	ensureBufrexRawEquals(test, msg);
	ensureBufrexRawEquals(test, msg1);

	/* Ensure that the decoded strings are zero-padded */
	gen_ensure_equals(memcmp(dba_var_value(msg1->subsets[0]->vars[0]), "abcdefg\0\0\0\0\0\0\0", 7+7), 0);

	bufrex_msg_delete(msg);
	bufrex_msg_delete(msg1);
}

// Encode a BUFR with an optional section
template<> template<>
void to::test<2>()
{
	bufrex_msg msg;

	CHECKED(bufrex_msg_create(BUFREX_BUFR, &msg));

	/* Initialise common message bits */
	msg->edition = 3;            // BUFR ed.4
	msg->type = 0;               // Template 8.255.171
	msg->subtype = 255;
	msg->localsubtype = 0;
	msg->opt.bufr.centre = 98;
	msg->opt.bufr.subcentre = 0;
	msg->opt.bufr.master_table = 12;
	msg->opt.bufr.local_table = 1;
	msg->opt.bufr.compression = 1;
	msg->opt.bufr.optional_section_length = 5;
	msg->opt.bufr.optional_section = strdup("Ciao");
	msg->rep_year = 2008;
	msg->rep_month = 5;
	msg->rep_day = 3;
	msg->rep_hour = 12;
	msg->rep_minute = 30;
	msg->rep_second = 0;

	/* Load encoding tables */
	CHECKED(bufrex_msg_load_tables(msg));

	/* Fill up the data descriptor section */
	CHECKED(bufrex_msg_append_datadesc(msg, DBA_VAR(0,  0,  13)));

	/* Get the working subset */
	bufrex_subset s;
	CHECKED(bufrex_msg_get_subset(msg, 0, &s));

	/* Set a text variable */
	CHECKED(bufrex_subset_store_variable_c(s, DBA_VAR(0, 0, 13), "12345678901234567890"));

	/* Set it to a shorter text, to see if the encoder encodes the trailing garbage */
	CHECKED(dba_var_setc(s->vars[0], "abcdefg"));

	/* Encode */
	dba_rawmsg rmsg = NULL;
	CHECKED(bufrex_msg_encode(msg, &rmsg));

	// Ensure that the encoded strings are space-padded
	gen_ensure(memfind(rmsg, "abcdefg       ", 14));

	// Decode the message
	bufrex_msg msg1;
	CHECKED(bufrex_msg_create(BUFREX_BUFR, &msg1));
	CHECKED(bufrex_msg_decode(msg1, rmsg));

	dba_rawmsg_delete(rmsg);

	gen_ensure_equals(msg1->opt.bufr.optional_section_length, 6); // Has been padded
	gen_ensure_equals(memcmp(msg1->opt.bufr.optional_section, "Ciao\0", 6), 0);

	TestBufrexMsg test;
	test.edition = 3;
	test.cat = 0;
	test.subcat = 255;
	test.localsubcat = 0;
	test.subsets = 1;
	test.subset(0).vars = 1;
	test.subset(0).set(DBA_VAR(0, 0, 13), "abcdefg");
	ensureBufrexRawEquals(test, msg);
	ensureBufrexRawEquals(test, msg1);

	/* Ensure that the decoded strings are zero-padded */
	gen_ensure_equals(memcmp(dba_var_value(msg1->subsets[0]->vars[0]), "abcdefg\0\0\0\0\0\0\0", 7+7), 0);

	bufrex_msg_delete(msg);
	bufrex_msg_delete(msg1);
}

// Test variable ranges during encoding
template<> template<>
void to::test<3>()
{
	bufrex_msg msg;

	CHECKED(bufrex_msg_create(BUFREX_BUFR, &msg));

	/* Initialise common message bits */
	msg->edition = 3;            // BUFR ed.4
	msg->type = 0;               // Template 8.255.171
	msg->subtype = 255;
	msg->localsubtype = 0;
	msg->opt.bufr.centre = 98;
	msg->opt.bufr.subcentre = 0;
	msg->opt.bufr.master_table = 12;
	msg->opt.bufr.local_table = 1;
	msg->opt.bufr.compression = 0;
	msg->rep_year = 2008;
	msg->rep_month = 5;
	msg->rep_day = 3;
	msg->rep_hour = 12;
	msg->rep_minute = 30;
	msg->rep_second = 0;

	/* Load encoding tables */
	CHECKED(bufrex_msg_load_tables(msg));

	/* Fill up the data descriptor section */
	CHECKED(bufrex_msg_append_datadesc(msg, DBA_VAR(0, 1, 1)));

	/* Get the working subset */
	bufrex_subset s;
	CHECKED(bufrex_msg_get_subset(msg, 0, &s));

	/* Set the test variable */
	//CHECKED(bufrex_subset_store_variable_d(s, DBA_VAR(0, 1, 1), -1.0));
	/* Now it errors here, because the range check is appropriately strict */
	dba_err err = bufrex_subset_store_variable_d(s, DBA_VAR(0, 1, 1), -1.0);
	ensure(err == DBA_ERROR);

#if 0
	/* Encode gives error because of overflow */
	dba_rawmsg rmsg = NULL;
	dba_err err = bufrex_msg_encode(msg, &rmsg);
	ensure(err == DBA_ERROR);
#endif
}



}

/* vim:set ts=4 sw=4: */
