# Failed experiment: the __dict__ of MaskedArray is a dictproxy instance that
# is read-only
import dballe.volnd
import MA, numpy
import rpy

#dballe.volnd.Data.__dict__['as_r'] = new.instancemethod(fun, None, dballe.volnd.Data)

def genfloat(a):
	for x in a.flat:
		if MA.getmask(x) != 1:
			yield float(x)
		else:
			yield rpy.r.NAN

def ma_to_r(arr, dimnames=None):
	"""
	Convert to an R object
	"""

	vlen = numpy.prod(arr.shape)

	### This strategy fails as it gets converted as a recursive list and
	### not an array
	## To compensate for unconvertible typecodes, various fill
	## values and whatnot, we just create a new array filling in
	## with proper NaNs as needed
	#var = numpy.fromiter(genfloat(arr), dtype=float, count=len)
	#var.shape = arr.shape
	#rpy.r.print_(var)
	#return var

	# Trouble:
	#  - a numpy array is converted as a list of lists
	#  - I found no way to assign to an element in an R array
	#  - cannot use a tuple to index a numpy/MA array

	oldmode = rpy.r.rep.local_mode()
	rpy.r.rep.local_mode(rpy.NO_CONVERSION)
	rpy.r.array.local_mode(rpy.NO_CONVERSION)
	rpy.r.aperm.local_mode(rpy.NO_CONVERSION)
	vec = rpy.r.rep(rpy.r.NAN, vlen)
	#print "pre",; rpy.r.print_(vec)
	for i, x in enumerate(arr.flat):
		if MA.getmask(x) != 1:
			vec[i] = float(x)
	#print "post",; rpy.r.print_(vec)
	#return rpy.r.array(data=vec, dim=[i for i in arr.shape])
	if dimnames:
		return rpy.r.aperm(rpy.r.array(data=vec, dim=[i for i in reversed(arr.shape)], dimnames=[i for i in reversed(dimnames)]), perm=[i for i in reversed(range(1,len(arr.shape)+1))])
	else:
		return rpy.r.aperm(rpy.r.array(data=vec, dim=[i for i in reversed(arr.shape)]), perm=[i for i in reversed(range(1,len(arr.shape)+1))])

def vnddata_to_r(data):
	dn = []
	for i in data.dims:
		dn.append(map(str, i))
	return ma_to_r(data.vals, dimnames=dn)
