#include <dballe++/record.h>

using namespace std;

namespace dballe {

void Record::dumpToStderr()
{
	dba_record_print(m_rec, stderr);
}

}

#ifdef DBALLEPP_COMPILE_TESTSUITE

#include <tests/test-utils.h>

namespace tut {

struct record_shar {
};

TESTGRP( record );

using namespace dballe;

template<> template<>
void to::test<1>()
{
	Record rec;

	rec.keySet(DBA_KEY_BLOCK, 3);
	gen_ensure_equals(rec.keyEnqi(DBA_KEY_BLOCK), 3);
	gen_ensure_equals(rec.keyEnq(DBA_KEY_BLOCK).enqi(), 3);

	rec.varSet(DBA_VAR(0, 4, 1), 2001);
	gen_ensure_equals(rec.varEnqi(DBA_VAR(0, 4, 1)), 2001);
	gen_ensure_equals(rec.varEnq(DBA_VAR(0, 4, 1)).enqi(), 2001);

	int count = 0;
	for (Record::iterator i = rec.begin(); i != rec.end(); ++i)
	{
		Var v(*i);
		gen_ensure_equals(v.code(), DBA_VAR(0, 4, 1));
		++count;
	}
	gen_ensure_equals(count, 1);
}

// Check simplified operators
template<> template<>
void to::test<2>()
{
	Record record;

	gen_ensure(!record.contains("block"));
	record.set("block", 4);
	gen_ensure(record.contains("block"));
	gen_ensure_equals(record.enqi("block"), 4);

	record.unset("block");
	gen_ensure(!record.contains("block"));

	gen_ensure(!record.contains("B01001"));
	record.set("B01001", 4);
	gen_ensure(record.contains("B01001"));
	gen_ensure_equals(record.enqi("B01001"), 4);

	record.unset("block");
	gen_ensure(!record.contains("block"));

}

// Check setAnaContext
template<> template<>
void to::test<3>()
{
	Record rec;
	rec.setAnaContext();
	gen_ensure(!rec.contains("ana_id"));
	gen_ensure_equals(rec.enqi("year"), 1000);
	gen_ensure_equals(rec.enqi("month"), 1);
	gen_ensure_equals(rec.enqi("day"), 1);
	gen_ensure_equals(rec.enqi("hour"), 0);
	gen_ensure_equals(rec.enqi("min"), 0);
	gen_ensure_equals(rec.enqi("sec"), 0);
	gen_ensure_equals(rec.enqi("leveltype"), 257);
	gen_ensure_equals(rec.enqi("l1"), 0);
	gen_ensure_equals(rec.enqi("l2"), 0);
	gen_ensure_equals(rec.enqi("pindicator"), 0);
	gen_ensure_equals(rec.enqi("p1"), 0);
	gen_ensure_equals(rec.enqi("p2"), 0);
	gen_ensure_equals(rec.enqi("rep_cod"), 254);
}

// Try out all copying functions
template<> template<>
void to::test<4>()
{
	Record master;
	master.set("block", 4);
	master.set("latmin", 4.1234);
	master.set("B01001", 4);

	{
		Record r1 = master;
		gen_ensure_equals(r1.enqi("block"), 4);
		gen_ensure_equals(r1.enqd("latmin"), 4.1234);
		gen_ensure_equals(r1.enqi("B01001"), 4);
	}

	Record r2 = master.copy();
	gen_ensure_equals(r2.enqi("block"), 4);
	gen_ensure_equals(r2.enqd("latmin"), 4.1234);
	gen_ensure_equals(r2.enqi("B01001"), 4);

	Record r3 = r2;
	gen_ensure_equals(r3.enqi("block"), 4);
	gen_ensure_equals(r3.enqd("latmin"), 4.1234);
	gen_ensure_equals(r3.enqi("B01001"), 4);
	r2.unset("latmin");
	gen_ensure_equals(r3.enqd("latmin"), 4.1234);
	r3.setd("latmin", 4.3214);
	gen_ensure_equals(r3.enqd("latmin"), 4.3214);

	r3 = r3;
	gen_ensure_equals(r3.enqi("block"), 4);
	gen_ensure_equals(r3.enqd("latmin"), 4.3214);
	gen_ensure_equals(r3.enqi("B01001"), 4);

	master = r3;
	gen_ensure_equals(master.enqi("block"), 4);
	gen_ensure_equals(master.enqd("latmin"), 4.3214);
	gen_ensure_equals(master.enqi("B01001"), 4);
}

// This caused a segfault
template<> template<>
void to::test<5>()
{
	Record rec;
	rec.setc("query", "nosort");
	Record rec1 = rec.copy();
	rec1.setc("query", "nosort");
}

// Test equals()
template<> template<>
void to::test<6>()
{
	Record rec;
	rec.setd("lat", 1.2345);
	rec.seti("B01001", 4);

	Record rec1 = rec.copy();
	gen_ensure(rec.equals(rec1));
	gen_ensure(rec1.equals(rec));
	rec1.setd("lat", 5.4321);
	gen_ensure(!rec.equals(rec1));
	gen_ensure(!rec1.equals(rec));

	rec1 = rec.copy();
	gen_ensure(rec.equals(rec1));
	gen_ensure(rec1.equals(rec));
	rec1.unset("lat");
	gen_ensure(!rec.equals(rec1));
	gen_ensure(!rec1.equals(rec));

	rec1 = rec.copy();
	gen_ensure(rec.equals(rec1));
	gen_ensure(rec1.equals(rec));
	rec1.seti("B01001", 3);
	gen_ensure(!rec.equals(rec1));
	gen_ensure(!rec1.equals(rec));

	rec1 = rec.copy();
	gen_ensure(rec.equals(rec1));
	gen_ensure(rec1.equals(rec));
	rec1.unset("B01001");
	gen_ensure(!rec.equals(rec1));
	gen_ensure(!rec1.equals(rec));
}

}

#endif
