/*
    File: exprilfs.h
    Auth: Brian Allen Vanderburg II
    Date: Tuesday, February 28, 2006
    Desc: Inline Function Solvers for exprEvalNode

    This file is part of ExprEval.
*/

/*
    This is here to help prevent expreval.c from getting
    too crowded.

    Provided variables:
    obj: expression object point
    nodes: function node with paramters
    d1, d2: variables
    err: error
    val: value pointer for resuld
    pos: integer

    Also EXPR_RESET_ERR() and EXPR_CHECK_ERR()

    The chunks below are included inside a statement that looks like this:

    switch(nodes->data.function.type)
        {
        #include "exprilfs.h"

        default:
            {
            return EXPR_ERROR_UNKNOWN;
            }
        }
*/



/* abs */
case EXPR_NODEFUNC_ABS:
{
	err = exprEvalNode(obj, nodes->data.function.nodes, 0, &d1);

	if (!err) {
		if (d1 >= 0)
			*val = d1;
		else
			*val = -d1;
	} else
		return err;

	break;
}

/* mod */
case EXPR_NODEFUNC_MOD:
{
	err = exprEvalNode(obj, nodes->data.function.nodes, 0, &d1);

	if (!err)
		err = exprEvalNode(obj, nodes->data.function.nodes, 1, &d2);

	if (!err) {
		EXPR_RESET_ERR();
		*val = fmod(d1, d2);
		EXPR_CHECK_ERR();
	} else
		return err;

	break;
}

/* ipart */
case EXPR_NODEFUNC_IPART:
{
	err = exprEvalNode(obj, nodes->data.function.nodes, 0, &d1);

	if (!err) {
		EXPR_RESET_ERR();
		modf(d1, val);
		EXPR_CHECK_ERR();
	} else
		return err;

	break;
}

/* fpart */
case EXPR_NODEFUNC_FPART:
{
	err = exprEvalNode(obj, nodes->data.function.nodes, 0, &d1);

	if (!err) {
		EXPR_RESET_ERR();
		*val = modf(d1, &d2);
		EXPR_CHECK_ERR();
	} else
		return err;

	break;
}

/* min */
case EXPR_NODEFUNC_MIN:
{
	err = exprEvalNode(obj, nodes->data.function.nodes, 0, &d1);

	if (!err) {
		for (pos = 1; pos < nodes->data.function.nodecount; pos++) {
			err = exprEvalNode(obj, nodes->data.function.nodes, pos, &d2);
			if (!err) {
				if (d2 < d1)
					d1 = d2;
			} else
				return err;
		}
	} else
		return err;

	*val = d1;

	break;
}

/* max */
case EXPR_NODEFUNC_MAX:
{
	int tmppos;

	err = exprEvalNode(obj, nodes->data.function.nodes, 0, &d1);

	if (!err) {
		for (tmppos = 1; tmppos < nodes->data.function.nodecount; tmppos++) {
			err = exprEvalNode(obj, nodes->data.function.nodes, tmppos, &d2);
			if (!err) {
				if (d2 > d1)
					d1 = d2;
			} else
				return err;
		}
	} else
		return err;

	*val = d1;

	break;
}

/* pow */
case EXPR_NODEFUNC_POW:
{
	err = exprEvalNode(obj, nodes->data.function.nodes, 0, &d1);

	if (!err)
		err = exprEvalNode(obj, nodes->data.function.nodes, 1, &d2);

	if (!err) {
		EXPR_RESET_ERR();
		*val = pow(d1, d2);
		EXPR_CHECK_ERR();
	} else
		return err;

	break;
}

/* sqrt */
case EXPR_NODEFUNC_SQRT:
{
	err = exprEvalNode(obj, nodes->data.function.nodes, 0, &d1);

	if (!err) {
		EXPR_RESET_ERR();
		*val = sqrt(d1);
		EXPR_CHECK_ERR();
	} else
		return err;

	break;
}

/* sin */
case EXPR_NODEFUNC_SIN:
{
	err = exprEvalNode(obj, nodes->data.function.nodes, 0, &d1);

	if (!err) {
		EXPR_RESET_ERR();
		*val = sin(d1);
		EXPR_CHECK_ERR();
	} else
		return err;

	break;
}

/* sinh */
case EXPR_NODEFUNC_SINH:
{
	err = exprEvalNode(obj, nodes->data.function.nodes, 0, &d1);

	if (!err) {
		EXPR_RESET_ERR();
		*val = sinh(d1);
		EXPR_CHECK_ERR();
	} else
		return err;

	break;
}

/* asin */
case EXPR_NODEFUNC_ASIN:
{
	err = exprEvalNode(obj, nodes->data.function.nodes, 0, &d1);

	if (!err) {
		EXPR_RESET_ERR();
		*val = asin(d1);
		EXPR_CHECK_ERR();
	} else
		return err;

	break;
}

/* cos */
case EXPR_NODEFUNC_COS:
{
	err = exprEvalNode(obj, nodes->data.function.nodes, 0, &d1);

	if (!err) {
		EXPR_RESET_ERR();
		*val = cos(d1);
		EXPR_CHECK_ERR();
	} else
		return err;

	break;
}

/* cosh */
case EXPR_NODEFUNC_COSH:
{
	err = exprEvalNode(obj, nodes->data.function.nodes, 0, &d1);

	if (!err) {
		EXPR_RESET_ERR();
		*val = cosh(d1);
		EXPR_CHECK_ERR();
	} else
		return err;

	break;
}

/* acos */
case EXPR_NODEFUNC_ACOS:
{
	err = exprEvalNode(obj, nodes->data.function.nodes, 0, &d1);

	if (!err) {
		EXPR_RESET_ERR();
		*val = acos(d1);
		EXPR_CHECK_ERR();
	} else
		return err;

	break;
}

/* tan */
case EXPR_NODEFUNC_TAN:
{
	err = exprEvalNode(obj, nodes->data.function.nodes, 0, &d1);

	if (!err) {
		EXPR_RESET_ERR();
		*val = tan(d1);
		EXPR_CHECK_ERR();
	} else
		return err;

	break;
}

/* tanh */
case EXPR_NODEFUNC_TANH:
{
	err = exprEvalNode(obj, nodes->data.function.nodes, 0, &d1);

	if (!err) {
		EXPR_RESET_ERR();
		*val = tanh(d1);
		EXPR_CHECK_ERR();
	} else
		return err;

	break;
}

/* atan */
case EXPR_NODEFUNC_ATAN:
{
	err = exprEvalNode(obj, nodes->data.function.nodes, 0, &d1);

	if (!err) {
		EXPR_RESET_ERR();
		*val = atan(d1);
		EXPR_CHECK_ERR();
	} else
		return err;

	break;
}

/* atan2 */
case EXPR_NODEFUNC_ATAN2:
{
	err = exprEvalNode(obj, nodes->data.function.nodes, 0, &d1);

	if (!err)
		err = exprEvalNode(obj, nodes->data.function.nodes, 1, &d2);

	if (!err) {
		EXPR_RESET_ERR();
		*val = atan2(d1, d2);
		EXPR_CHECK_ERR();
	} else
		return err;

	break;
}

/* log */
case EXPR_NODEFUNC_LOG:
{
	err = exprEvalNode(obj, nodes->data.function.nodes, 0, &d1);

	if (!err) {
		EXPR_RESET_ERR();
		*val = log10(d1);
		EXPR_CHECK_ERR();
	} else
		return err;

	break;
}

/* pow10 */
case EXPR_NODEFUNC_POW10:
{
	err = exprEvalNode(obj, nodes->data.function.nodes, 0, &d1);

	if (!err) {
		EXPR_RESET_ERR();
		*val = pow(10.0, d1);
		EXPR_CHECK_ERR();
	} else
		return err;

	break;
}

/* ln */
case EXPR_NODEFUNC_LN:
{
	err = exprEvalNode(obj, nodes->data.function.nodes, 0, &d1);

	if (!err) {
		EXPR_RESET_ERR();
		*val = log(d1);
		EXPR_CHECK_ERR();
	} else
		return err;

	break;
}

/* exp */
case EXPR_NODEFUNC_EXP:
{
	err = exprEvalNode(obj, nodes->data.function.nodes, 0, &d1);

	if (!err) {
		EXPR_RESET_ERR();
		*val = exp(d1);
		EXPR_CHECK_ERR();
	} else
		return err;

	break;
}

case EXPR_NODEFUNC_LOGN:
{
	EXPRTYPE l1, l2;

	err = exprEvalNode(obj, nodes->data.function.nodes, 0, &d1);

	if (!err)
		err = exprEvalNode(obj, nodes->data.function.nodes, 1, &d2);

	if (!err) {
		EXPR_RESET_ERR();
		l1 = log(d1);
		EXPR_CHECK_ERR();
		l2 = log(d2);
		EXPR_CHECK_ERR();


		if (l2 == 0.0) {
#if(EXPR_ERROR_LEVEL >= EXPR_ERROR_LEVEL_CHECK)
			return EXPR_ERROR_OUTOFRANGE;
#else
			*val = 0.0;
			return EXPR_ERROR_NOERROR;
#endif
		}

		*val = l1 / l2;
	} else
		return err;

	break;
}

/* ceil */
case EXPR_NODEFUNC_CEIL:
{
	err = exprEvalNode(obj, nodes->data.function.nodes, 0, &d1);

	if (!err) {
		*val = ceil(d1);
	} else
		return err;

	break;
}

/* floor */
case EXPR_NODEFUNC_FLOOR:
{
	err = exprEvalNode(obj, nodes->data.function.nodes, 0, &d1);

	if (!err) {
		*val = floor(d1);
	} else
		return err;

	break;
}

/* rand */
case EXPR_NODEFUNC_RAND:
{
	long a;

	/* Perform random routine directly */
	a = ((long) (*(nodes->data.function.refs[0]))) * 214013L + 2531011L;
	*(nodes->data.function.refs[0]) = (EXPRTYPE) a;

	*val = (EXPRTYPE) ((a >> 16) & 0x7FFF) / (EXPRTYPE) (32768);
	break;
}

/* random */
case EXPR_NODEFUNC_RANDOM:
{
	EXPRTYPE diff, rval;
	long a;

	err = exprEvalNode(obj, nodes->data.function.nodes, 0, &d1);

	if (!err)
		err = exprEvalNode(obj, nodes->data.function.nodes, 1, &d2);

	if (!err) {
		diff = d2 - d1;

		/* Perform random routine directly */
		a = ((long) (*(nodes->data.function.refs[0]))) * 214013L + 2531011L;
		*(nodes->data.function.refs[0]) = (EXPRTYPE) a;

		rval = (EXPRTYPE) ((a >> 16) & 0x7FFF) / (EXPRTYPE) (32767);

		*val = (rval * diff) + d1;
	} else
		return err;

	break;
}

/* randomize */
case EXPR_NODEFUNC_RANDOMIZE:
{
	static int curcall = 0;

	curcall++;

	*(nodes->data.function.refs[0]) = (EXPRTYPE) ((clock() + 1024 + curcall) * time(NULL));

	break;
}

/* deg */
case EXPR_NODEFUNC_DEG:
{
	err = exprEvalNode(obj, nodes->data.function.nodes, 0, &d1);

	if (!err) {
		*val = (180.0 * d1) / M_PI;
	} else
		return err;

	break;
}

/* rad */
case EXPR_NODEFUNC_RAD:
{
	err = exprEvalNode(obj, nodes->data.function.nodes, 0, &d1);

	if (!err) {
		*val = (M_PI * d1) / 180.0;
	} else
		return err;

	break;
}

/* recttopolr */
case EXPR_NODEFUNC_RECTTOPOLR:
{
	err = exprEvalNode(obj, nodes->data.function.nodes, 0, &d1);

	if (!err)
		err = exprEvalNode(obj, nodes->data.function.nodes, 1, &d2);

	if (!err) {
		EXPR_RESET_ERR();
		*val = sqrt((d1 * d1) + (d2 * d2));
		EXPR_CHECK_ERR();
	} else
		return err;

	break;
}

/* recttopola */
case EXPR_NODEFUNC_RECTTOPOLA:
{
	EXPRTYPE tmp;

	err = exprEvalNode(obj, nodes->data.function.nodes, 0, &d1);

	if (!err)
		err = exprEvalNode(obj, nodes->data.function.nodes, 1, &d2);

	if (!err) {
		EXPR_RESET_ERR();
		tmp = atan2(d2, d1);
		EXPR_CHECK_ERR();

		if (tmp < 0.0)
			*val = tmp = (2.0 * M_PI);
		else
			*val = tmp;
	} else
		return err;

	break;
}

/* poltorectx */
case EXPR_NODEFUNC_POLTORECTX:
{
	err = exprEvalNode(obj, nodes->data.function.nodes, 0, &d1);

	if (!err)
		err = exprEvalNode(obj, nodes->data.function.nodes, 1, &d2);

	if (!err) {
		EXPR_RESET_ERR();
		*val = d1 * cos(d2);
		EXPR_CHECK_ERR();
	} else
		return err;

	break;
}

/* poltorecty */
case EXPR_NODEFUNC_POLTORECTY:
{
	err = exprEvalNode(obj, nodes->data.function.nodes, 0, &d1);

	if (!err)
		err = exprEvalNode(obj, nodes->data.function.nodes, 1, &d2);

	if (!err) {
		EXPR_RESET_ERR();
		*val = d1 * sin(d2);
		EXPR_CHECK_ERR();
	} else
		return err;

	break;
}

/* if */
case EXPR_NODEFUNC_IF:
{
	err = exprEvalNode(obj, nodes->data.function.nodes, 0, &d1);

	if (!err) {
		if (d1 != 0.0) {
			err = exprEvalNode(obj, nodes->data.function.nodes, 1, val);
			if (err)
				return err;
		} else {
			err = exprEvalNode(obj, nodes->data.function.nodes, 2, val);
			if (err)
				return err;
		}
	} else
		return err;

	break;
}

/* select */
case EXPR_NODEFUNC_SELECT:
{
	err = exprEvalNode(obj, nodes->data.function.nodes, 0, &d1);

	if (!err) {
		if (d1 < 0.0) {
			err = exprEvalNode(obj, nodes->data.function.nodes, 1, val);
			if (err)
				return err;
		} else if (d1 == 0.0) {
			err = exprEvalNode(obj, nodes->data.function.nodes, 2, val);
			if (err)
				return err;
		} else {
			if (nodes->data.function.nodecount == 3) {
				err = exprEvalNode(obj, nodes->data.function.nodes, 2, val);
				if (err)
					return err;
			} else {
				err = exprEvalNode(obj, nodes->data.function.nodes, 3, val);
				if (err)
					return err;
			}
		}
	} else
		return err;

	break;
}

/* equal */
case EXPR_NODEFUNC_EQUAL:
{
	err = exprEvalNode(obj, nodes->data.function.nodes, 0, &d1);

	if (!err)
		err = exprEvalNode(obj, nodes->data.function.nodes, 1, &d2);

	if (!err) {
		*val = (d1 == d2) ? 1.0 : 0.0;
	} else
		return err;

	break;
}

/* above */
case EXPR_NODEFUNC_ABOVE:
{
	err = exprEvalNode(obj, nodes->data.function.nodes, 0, &d1);

	if (!err)
		err = exprEvalNode(obj, nodes->data.function.nodes, 1, &d2);

	if (!err) {
		*val = (d1 > d2) ? 1.0 : 0.0;
	} else
		return err;

	break;
}

/* below */
case EXPR_NODEFUNC_BELOW:
{
	err = exprEvalNode(obj, nodes->data.function.nodes, 0, &d1);

	if (!err)
		err = exprEvalNode(obj, nodes->data.function.nodes, 1, &d2);

	if (!err) {
		*val = (d1 < d2) ? 1.0 : 0.0;
	} else
		return err;

	break;
}

/* avg */
case EXPR_NODEFUNC_AVG:
{
	d2 = 0.0;

	for (pos = 0; pos < nodes->data.function.nodecount; pos++) {
		err = exprEvalNode(obj, nodes->data.function.nodes, pos, &d1);
		if (!err) {
			d2 += d1;
		} else
			return err;
	}

	*val = d2 / (EXPRTYPE) (nodes->data.function.nodecount);

	break;
}

/* clip */
case EXPR_NODEFUNC_CLIP:
{
	EXPRTYPE v;

	err = exprEvalNode(obj, nodes->data.function.nodes, 0, &v);

	if (!err)
		err = exprEvalNode(obj, nodes->data.function.nodes, 1, &d1);

	if (!err)
		err = exprEvalNode(obj, nodes->data.function.nodes, 1, &d2);

	if (!err) {
		if (v < d1)
			*val = d1;
		else if (v > d2)
			*val = d2;
		else
			*val = v;
	} else
		return err;

	break;
}

/* clamp */
case EXPR_NODEFUNC_CLAMP:
{
	EXPRTYPE v, tmp;

	err = exprEvalNode(obj, nodes->data.function.nodes, 0, &v);

	if (!err)
		err = exprEvalNode(obj, nodes->data.function.nodes, 1, &d1);

	if (!err)
		err = exprEvalNode(obj, nodes->data.function.nodes, 2, &d2);

	if (!err) {
		EXPR_RESET_ERR();
		tmp = fmod(v - d1, d2 - d1);
		EXPR_CHECK_ERR();

		if (tmp < 0.0)
			*val = tmp + d2;
		else
			*val = tmp + d1;
	} else
		return err;

	break;
}

/* pntchange */
case EXPR_NODEFUNC_PNTCHANGE:
{
	EXPRTYPE n1, n2, pnt;
	EXPRTYPE odiff, ndiff, perc;

	err = exprEvalNode(obj, nodes->data.function.nodes, 0, &d1);

	if (!err)
		err = exprEvalNode(obj, nodes->data.function.nodes, 1, &d2);

	if (!err)
		err = exprEvalNode(obj, nodes->data.function.nodes, 2, &n1);

	if (!err)
		err = exprEvalNode(obj, nodes->data.function.nodes, 3, &n2);

	if (!err)
		err = exprEvalNode(obj, nodes->data.function.nodes, 4, &pnt);

	if (!err) {
		odiff = d2 - d1;
		ndiff = n2 - n1;

		if (odiff == 0.0) {
			*val = d1;
			return EXPR_ERROR_NOERROR;
		}

		perc = (pnt - d1) / odiff;

		*val = n1 + (perc * ndiff);
	} else
		return err;

	break;
}

/* poly */
case EXPR_NODEFUNC_POLY:
{
	EXPRTYPE total, curpow;

	err = exprEvalNode(obj, nodes->data.function.nodes, 0, &d1);

	if (!err) {
		curpow = (EXPRTYPE) (nodes->data.function.nodecount) - 2.0;
		total = 0.0;

		for (pos = 1; pos < nodes->data.function.nodecount; pos++) {
			err = exprEvalNode(obj, nodes->data.function.nodes, pos, &d2);
			if (err)
				return err;

			EXPR_RESET_ERR();
			total = total + (d2 * pow(d1, curpow));
			EXPR_CHECK_ERR();

			curpow = curpow - 1.0;
		}
	} else
		return err;

	*val = total;
	break;
}

/* and */
case EXPR_NODEFUNC_AND:
{
	err = exprEvalNode(obj, nodes->data.function.nodes, 0, &d1);

	if (!err)
		err = exprEvalNode(obj, nodes->data.function.nodes, 1, &d2);

	if (!err) {
		if (d1 == 0.0 || d2 == 0.0)
			*val = 0.0;
		else
			*val = 1.0;
	} else
		return err;

	break;
}

/* or */
case EXPR_NODEFUNC_OR:
{
	err = exprEvalNode(obj, nodes->data.function.nodes, 0, &d1);

	if (!err)
		err = exprEvalNode(obj, nodes->data.function.nodes, 1, &d2);

	if (!err) {
		if (d1 != 0.0 || d2 != 0.0)
			*val = 1.0;
		else
			*val = 0.0;
	} else
		return err;

	break;
}

/* not */
case EXPR_NODEFUNC_NOT:
{
	err = exprEvalNode(obj, nodes->data.function.nodes, 0, &d1);

	if (!err) {
		if (d1 != 0.0)
			*val = 0.0;
		else
			*val = 1.0;
	} else
		return err;

	break;
}

/* for */
case EXPR_NODEFUNC_FOR:
{
	int tmppos;
	EXPRTYPE test;

	err = exprEvalNode(obj, nodes->data.function.nodes, 0, &d1);

	if (!err)
		err = exprEvalNode(obj, nodes->data.function.nodes, 1, &test);

	if (!err) {
		while (test != 0.0) {
			for (tmppos = 3; tmppos < nodes->data.function.nodecount; tmppos++) {
				err = exprEvalNode(obj, nodes->data.function.nodes, tmppos, val);
				if (err)
					return err;
			}

			err = exprEvalNode(obj, nodes->data.function.nodes, 2, &d1);
			if (err)
				return err;

			err = exprEvalNode(obj, nodes->data.function.nodes, 1, &test);
			if (err)
				return err;
		}
	} else
		return err;

	break;
}

/* many */
case EXPR_NODEFUNC_MANY:
{
	for (pos = 0; pos < nodes->data.function.nodecount; pos++) {
		err = exprEvalNode(obj, nodes->data.function.nodes, pos, val);
		if (err)
			return err;
	}

	break;
}
