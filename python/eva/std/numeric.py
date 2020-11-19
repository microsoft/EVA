# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license.
from eva import py_to_eva

def horizontal_sum(x):
	""" Sum together all elements of a vector. The result is replicated in all
		elements of the returned vector.
	
		Parameters
		----------
		x : an EVA compatible type (see eva.py_to_eva)
			The vector to sum together
		"""

	x = py_to_eva(x)
	i = 1
	while i < x.program.vec_size:
		y = x << i
		x = x + y
		i <<= 1
	return x
