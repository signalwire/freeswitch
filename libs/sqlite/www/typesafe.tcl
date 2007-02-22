set rcsid {$Id: different.tcl,v 1.7 2006/05/11 13:33:15 drh Exp $}
source common.tcl
header {SQLite Is Typesafe}
puts {
<p>
In December of 2006, 
<a href="http://en.wikipedia.org/">wikipedia</a> defines 
<a href="http://en.wikipedia.org/wiki/Type_safety">typesafe</a>
to a property of programming languages that detects and prevents
"type errors".
The wikipedia says:
</p>

<blockquote>
The behaviors classified as type errors by any given programming language
are generally those that result from attempts to perform on some value
(or values) an operation that is not appropriate to its type (or their
types). The fundamental basis for this classification is to a certain
 extent a matter of opinion: some language designers and programmers 
take the view that any operation not leading to program crashes, 
security flaws or other obvious failures is legitimate and need not be 
considered an error, while others consider any contravention of the
programmer's intent (as communicated via typing annotations) to be
erroneous and deserving of the label "unsafe".
</blockquote>

<p>
We, the developers of SQLite, take the first and more liberal view of
type safety expressed above - specifically that anything that does
not result in a crash or security flaw or other obvious failures is
not a type error.
Given this viewpoint, SQLite is easily shown to be typesafe since 
almost all operations are valid and well-defined for operands of 
all datatypes and those few cases where an operation is only valid
for a subset of the available datatypes (example: inserting a
value into an INTEGER PRIMARY KEY column) type errors are detected and
reported at run-time prior to performing the operation.
</p>

<p>
Some commentators hold that type safety implies static typing.
SQLite uses dynamic typing and thus cannot be typesafe in the eyes
of those who believe that only a statically typed language can be
typesafe.  But we believe that static typing is a distinct property
from type safety.  To quote again from the
<a href="http://en.wikipedia.org/wiki/Type_safety">wikipedia</a>:
</p>

<blockquote>
[T]ype safety and dynamic typing are not mutually exclusive. A 
dynamically typed language can be seen as a statically-typed language
with a very permissive type system under which any syntactically 
correct program is well-typed; as long as its dynamic semantics 
ensures that no such program ever "goes wrong" in an appropriate 
sense, it satisfies the definition above and can be called type-safe.
</blockquote>




}
