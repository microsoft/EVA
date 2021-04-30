# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license.

from ._eva import *
import numbers
import psutil

# Find the number of CPU cores available to this process. This has to happen before Galois is initialized because it
# messes with the CPU affinity of the process.
_default_num_threads = len(psutil.Process().cpu_affinity())
# Initialize Galois here (trying to do it in the static initialization step of the native library hangs).
_global_guard = _eva._GaloisGuard()
# Set the default number of threads to use to match the cores.
set_num_threads(_default_num_threads)

_current_program = None
def _curr():
    """ Returns the EvaProgram that is currently in context """
    global _current_program
    if _current_program == None:
        raise RuntimeError("No Program in context")
    return _current_program

def _py_to_term(x, program):
    """ Maps supported types into EVA terms """
    if isinstance(x, Expr):
        return x.term
    elif isinstance(x, list):
        return program._make_dense_constant(x)
    elif isinstance(x, numbers.Number):
        return program._make_uniform_constant(x)
    elif isinstance(x, Term):
        return x
    else:
        raise TypeError("No conversion to Term available for " + str(x))

def py_to_eva(x, program = None):
    """ Maps supported types into EVA terms. May be used in library functions
        to provide uniform support for Expr instances and python types that 
        are convertible into constants in EVA programs.
    
        Parameters
        ----------
        x : eva.Expr, EVA native Term, list or a number
            The value to be converted to an Expr
        program : EvaProgram, optional
            The program a new term is created in (if necessary). If None then
            the program currently in context is used (again if necessary).
        """
    if isinstance(x, Expr):
        return x
    else:
        if program == None:
            program = _curr()
        return Expr(_py_to_term(x, program), program)

class Expr():
    """ Wrapper for EVA's native Term class. Provides operator overloads that
        create terms in the associated EvaProgram.
        
        Attributes
        ----------
        term
            The EVA native term
        program : eva.EVAProgram
            The program the wrapped term is in
        """

    def __init__(self, term, program):
        self.term = term
        self.program = program

    def __add__(self,other):
        """ Create a new addition term """
        return Expr(self.program._make_term(Op.Add, [self.term, _py_to_term(other, self.program)]), self.program)

    def __radd__(self,other):
        """ Create a new addition term """
        return Expr(self.program._make_term(Op.Add, [_py_to_term(other, self.program), self.term]), self.program)

    def __sub__(self,other):
        """ Create a new subtraction term """
        return Expr(self.program._make_term(Op.Sub, [self.term, _py_to_term(other, self.program)]), self.program)

    def __rsub__(self,other):
        """ Create a new subtraction term """
        return Expr(self.program._make_term(Op.Sub, [_py_to_term(other, self.program), self.term]), self.program)

    def __mul__(self,other):
        """ Create a new multiplication term """
        return Expr(self.program._make_term(Op.Mul, [self.term, _py_to_term(other, self.program)]), self.program)

    def __rmul__(self,other):
        """ Create a new multiplication term """
        return Expr(self.program._make_term(Op.Mul, [_py_to_term(other, self.program), self.term]), self.program)

    def __pow__(self,exponent):
        """ Create exponentiation as nested multiplication terms """
        if exponent < 1:
            raise ValueError("exponent must be greater than zero, got " + exponent)
        result = self.term
        for i in range(exponent-1):
            result = self.program._make_term(Op.Mul, [result, self.term])
        return Expr(result, self.program)

    def __lshift__(self,rotation):
        """ Create a left rotation term """
        return Expr(self.program._make_left_rotation(self.term, rotation), self.program)

    def __rshift__(self,rotation):
        """ Create a right rotation term """
        return Expr(self.program._make_right_rotation(self.term, rotation), self.program)

    def __neg__(self):
        """ Create a negation term """
        return Expr(self.program._make_term(Op.Negate, [self.term]), self.program)

class EvaProgram(Program):
    """ A wrapper for EVA's native Program class. Acts as a context manager to
        set the program the Input and Output free functions operate on. """

    def __init__(self, name, vec_size):
        """ Create a new EvaProgram with a name and a vector size
            
            Parameters
            ----------
            name : str
                The name of the program
            vec_size : int
                The number of elements in all values in the program
                Must be a power-of-two
            """
        super().__init__(name, vec_size)

    def __enter__(self):
        global _current_program
        if _current_program != None:
            raise RuntimeError("There is already an EVA Program in context")
        _current_program = self
    
    def __exit__(self, exc_type, exc_value, exc_traceback):
        global _current_program
        if _current_program != self:
            raise RuntimeError("This program is not currently in context")
        _current_program = None

def Input(name, is_encrypted=True):
    """ Create a new named input term in the current EvaProgram

        Parameters
        ----------
        name : str
            The name of the input
        is_encrypted : bool, optional
            Whether this input should be encrypted or not (default: True)
        """
    program = _curr()
    return Expr(program._make_input(name, Type.Cipher if is_encrypted else Type.Raw), program)

def Output(name, expr):
    """ Create a new named output term in the current EvaProgram

        Parameters
        ----------
        name : str
            The name of the output
        is_encrypted : bool, optional
            Whether this input should be encrypted or not (default: True)
        """
    program = _curr()
    program._make_output(name, _py_to_term(expr, program))
