# Copyright (c) 1995  Colin Plumb.  All rights reserved.
# For licensing and other legal details, see the file legal.c.
#
# Assembly-language bignum primitives for the i960 Jx series.
#
# The Jx series is fairly straightforward single-instruction-issue 
# implementation, with a 1-cycle-issue 4-cycle-latency non-pipelined
# multiplier that we can use.  Note also that loads which hit in the
# cache have 2 cycles of latency and stores stall until all pending
# loads are done.
#
# What is intensely annoying about the i960 is that it uses the same
# flags for all conditional branches (even compare-and-branch sets the
# flags) AND for the carry bit.  Further, it is hard to manipulate
# that bit.
#
# Calling conventions:
# The r registers are all local, if you set them up.  There's an alternative
# calling convention that uses bal (branch and link) and doesn't set them up.
# Currently, all of these functions are designed to work that way.
# g0-g7 are argument registers and volatile across calls.  return in g0-g3.
# g8-g11 are extra argument registers, and volatile if used, but
#	preserved if not.  Here, they are not.
# g12 is used for PIC, and is preserved.
# g13 is a pointer to a structure return value, if used, and is volatile.
# g14 is magic, and is used as a return address in the branch-and-link
#	convention, and as a pointer to an argument block if the arguments
#	won't fit in registers, but is usually hardwired 0 and must be
#	returned set to zero (0).
# g15 is the frame pointer, and shouldn't be messed with.
# The AC (condition codes) are all volatile.
# The fp registers are all volatile, but irrelevant.
#

# BNWORD32
# lbnMultAdd1_32(BNWORD32 *out, BNWORD32 const *in, unsigned len, BNWORD32 k)
# This adds "k" * "in" to "len" words of "out" and returns the word of
# carry.
#
# For doing multiply-add, the 960 is a bit annoying because it uses
# the same status bits for the carry flag and for the loop indexing
# computation, and doesn't have an "add with carry out but not carry in"
# instruction.  Fortunately, we can arrange to have the loop indexing
# leave the carry bit clear most of the time.
#
# The basic sequence of the loop is:
# 1. Multiply k * *in++ -> high, low
# 2. Addc carry word and carry bit to low
# 3. Addc carry bit to high, producing carry word (note: cannot generate carry!)
# 4. Addc low to *out++
#
# Note that the carry bit set in step 4 is used in step 2.  The only place
# in this loop that the carry flag isn't in use is between steps 3 and 4,
# so we have to rotate the loop to place the loop indexing operations here.
# (Which consist of a compare-and-decrement and a conditional branch.)
# The loop above ignores the details of when to do loads and stores, which
# have some flexibility, but must be carefully scheduled to avoid stalls.
#
# The first iteration has no carry word in, so it requires only steps 1 and 4,
# and since we begin the loop with step 4, it boils down to just step 1
# followed by the loop indexing (which clears the carry bit in preparation
# for step 4).
#
# Arguments are passed as follows:
# g0 - out pointer
# g1 - in pointer
# g2 - length
# g3 - k
# The other registers are used as follows.
# g4 - low word of product
# g5 - high word of product
# g6 - current word of "out"
# g7 - carry word
# g13 - current word of "in"

	.globl _lbnMulAdd1_32
_lbnMulAdd1_32:
	ld	(g1),g13   	# Fetch *in
	addo	g1,4,g1   	# Increment in
	emul	g13,g3,g4	# Do multiply (step 1)
	ld	(g0),g6   	# Fetch *out
	chkbit	0,g2		# Check if loop counter was odd
	shro	1,g2,g2   	# Divide loop counter by 2
	mov	g5,g7		# Move high word to carry
	bno	ma_loop1	# If even, jump to ma_loop1
	cmpo	0,g2		# If odd, was it 1 (now 0)?
	be	ma_done   	# If equal (carry set), jump to ending code

# Entered with carry bit clear
ma_loop:
	ld	(g1),g13  	# Fetch *in
	addc	g4,g6,g6	# Add low to *out (step 4), generate carry
	emul	g13,g3,g4	# Do multiply (step 1)
	st	g6,(g0)  	# Write out *out
	addo	g0,4,g0  	# Increment out
	addo	g1,4,g1  	# Increment in
	ld	(g0),g6  	# Fetch next *out
	addc	g7,g4,g4	# Add carries to low (step 2)
	addc	g5,0,g7  	# Add carry bit to high (step 3) & clear carry
ma_loop1:
	ld	(g1),g13  	# Fetch *in
	addc	g4,g6,g6	# Add low to *out (step 4), generate carry
	emul	g13,g3,g4	# Do multiply (step 1)
	st	g6,(g0)  	# Write out *out
	addo	g0,4,g0  	# Increment out
	addo	g1,4,g1  	# Increment in
	ld	(g0),g6  	# Fetch next *out
	addc	g7,g4,g4	# Add carries to low (step 2)
	addc	g5,0,g7  	# Add carry bit to high (step 3) & clear carry

	cmpdeco	1,g2,g2
	bne	ma_loop
# When we come here, carry is *set*, and we stil have to do step 4
ma_done:
	cmpi	0,1		# Clear carry (equal flag)
	addc	g4,g6,g6	# Add low to *out (step 4), generate carry
	st	g6,(g0)   	# Write out *out
	addc	g7,0,g0   	# Add carry bit and word to produce return value
	ret

# Now, multiply N by 1 is similarly annoying.  We only have one add in the
# whole loop, which should just be able to leave its carry output in the
# carry flag for the next iteration, but we need the condition codes to do
# loop testing.  *Sigh*.
#
# void
# lbnMultN1_32(BNWORD32 *out, BNWORD32 const *in, unsigned len, BNWORD32 k)
# This stores len+1 words of "k" * len words of "in" and stores the result
# in "out".
#
# To avoid having to do a move after the first iteration, for the first
# step, g4/g5 is the product.  For second step, g6/g7 is used for product
# storage and g5 is the carry in.  It alternates from then on.
	.globl _lbnMulN1_32
_lbnMulN1_32:
	ld	(g1),g13 	# Fetch *in
	addo	g1,4,g1 	# Increment in
	emul	g13,g3,g4	# Do multiply (step 1)
	chkbit	0,g2		# Check if loop counter was odd
	shro	1,g2,g2  	# Divide loop counter by 2
	bno	m_loop1  	# If even, jump to ma_loop1
	mov	g4,g6
	cmpo	0,g2		# If counter was odd, was it 1 (now 0)?
	mov	g5,g7
	be	m_done		# If equal (carry set), jump to ending code

# Entered with carry bit clear
m_loop:
	# Result in g6, carry word in g7
	ld	(g1),g13	# Fetch *in
	addo	g1,4,g1 	# Increment in
	emul	g13,g3,g4	# Do multiply (step 1)
	st	g6,(g0) 	# Write out *out
	addo	g0,4,g0		# Increment out
	addc	g7,g4,g4	# Add carries to low (step 2)
# No need to add carry bit here, because it'll get remembered until next addc.
#	addc	g5,0,g5 	# Add carry bit to high (step 3)
m_loop1:
	# Carry word in g5
	ld	(g1),g13	# Fetch *in
	addo	g1,4,g1		# Increment in
	emul	g13,g3,g6	# Do multiply (step 1)
	st	g4,(g0)		# Write out *out
	addo	g0,4,g0 	# Increment out
	addc	g5,g6,g6	# Add carries to low (step 2)
	addc	g7,0,g7 	# Add carry bit to high (step 3)

	cmpdeco	1,g2,g2
	bne	m_loop

# When we come here, we have to store g6 and the carry word in g7.
m_done:
	st	g6,(g0) 	# Write out *out
	st	g7,4(g0)	# Write out *out
	ret

# BNWORD32
# lbnMultSub1_32(BNWORD32 *out, BNWORD32 const *in, unsigned len, BNWORD32 k)
# This subtracts "k" * "in" from "len" words of "out" and returns the word of
# borrow.
#
# This is similar to multiply-add, but actually a bit more obnoxious,
# because of the carry situation.  The 960 uses a carry (rather than a borrow)
# bit on subtracts, so the carry bit should be 1 for a subc to do the
# same thing as an ordinary subo.  So we use two carry chains: one from
# the add of the low-order words to the high-order carry word, and a second,
# which uses an extra register, to connect the subtracts.  This avoids
# the need to fiddle with inverting the bit in the usual case.
#
# Arguments are passed as follows:
# g0 - out pointer
# g1 - in pointer
# g2 - length
# g3 - k
# The other registers are used as follows.
# g4 - low word of product
# g5 - high word of product
# g6 - current word of "out"
# g7 - carry word
# g13 - current word of "in"
# g14 - remembered carry bit

	.globl _lbnMulSub1_32
_lbnMulSub1_32:
	ld	(g1),g13	# Fetch *in
	addo	g1,4,g1 	# Increment in
	emul	g13,g3,g4	# Do multiply (step 1)
	ld	(g0),g6 	# Fetch *out
	chkbit	0,g2    	# Check if loop counter was odd
	mov	1,g14   	# Set remembered carry for first iteration
	shro	1,g2,g2 	# Divide loop counter by 2
	mov	g5,g7   	# Move high word to carry
	bno	ms_loop1	# If even, jump to ma_loop1
	cmpo	0,g2    	# If odd, was it 1 (now 0)?
	be	ms_done 	# If equal (carry set), jump to ending code

# Entered with carry bit clear
ms_loop:
	ld	(g1),g13	# Fetch *in
	cmpi	g14,1   	# Set carry flag
	subc	g4,g6,g6	# Subtract low from *out (step 4), gen. carry
	emul	g13,g3,g4	# Do multiply (step 1)
	addc	0,0,g14 	# g14 = carry, then clear carry
	st	g6,(g0) 	# Write out *out
	addo	g0,4,g0 	# Increment out
	addo	g1,4,g1 	# Increment in
	ld	(g0),g6 	# Fetch next *out
	addc	g7,g4,g4	# Add carries to low (step 2)
	addc	g5,0,g7 	# Add carry bit to high (step 3)
ms_loop1:
	ld	(g1),g13	# Fetch *in
	cmpi	g14,1   	# Set carry flag for subtrsct
	subc	g4,g6,g6	# Subtract low from *out (step 4), gen. carry
	emul	g13,g3,g4	# Do multiply (step 1)
	addc	0,0,g14 	# g14 = carry, then clear carry
	st	g6,(g0) 	# Write out *out
	addo	g0,4,g0 	# Increment out
	addo	g1,4,g1 	# Increment in
	ld	(g0),g6 	# Fetch next *out
	addc	g7,g4,g4	# Add carries to low (step 2)
	addc	g5,0,g7 	# Add carry bit to high (step 3)

	cmpdeco	1,g2,g2
	bne	ms_loop
# When we come here, carry is *set*, and we stil have to do step 4
ms_done:
	cmpi	g14,1   	# set carry (equal flag)
	subc	g4,g6,g6	# Add low to *out (step 4), generate carry
	st	g6,(g0) 	# Write out *out
	subc	0,0,g14 	# g14 = -1 if no carry (borrow), 0 if carry
	subo	g14,g7,g0	# Add borrow bit to produce return value
	mov	0,g14   	# Restore g14 to 0 for return
	ret
