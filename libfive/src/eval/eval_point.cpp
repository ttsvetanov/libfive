/*
libfive: a CAD kernel for modeling with implicit functions
Copyright (C) 2017  Matt Keeter

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include <boost/numeric/interval.hpp>

#include "libfive/eval/eval_point.hpp"

namespace Kernel {

PointEvaluator::PointEvaluator(std::shared_ptr<Tape> t)
    : PointEvaluator(t, std::map<Tree::Id, float>())
{
    // Nothing to do here
}

PointEvaluator::PointEvaluator(
        std::shared_ptr<Tape> t, const std::map<Tree::Id, float>& vars)
    : BaseEvaluator(t, vars), f(tape->num_clauses + 1, 1)
{
    // Unpack variables into result array
    for (auto& v : t->vars.right)
    {
        auto var = vars.find(v.first);
        f(v.second) = (var != vars.end()) ? var->second : 0;
    }

    // Unpack constants into result array
    for (auto& c : tape->constants)
    {
        f(c.first) = c.second;
    }
}

float PointEvaluator::eval(const Eigen::Vector3f& pt)
{
    f(tape->X) = pt.x();
    f(tape->Y) = pt.y();
    f(tape->Z) = pt.z();

    for (auto& o : tape->oracles)
    {
        o->set(pt);
    }

    return f(tape->rwalk(*this));
}

std::pair<float, Tape::Handle> PointEvaluator::evalAndPush(
        const Eigen::Vector3f& pt)
{
    auto out = eval(pt);
    auto p = tape->push([&](Opcode::Opcode op, Clause::Id /* id */,
                  Clause::Id a, Clause::Id b)
    {
        // For min and max operations, we may only need to keep one branch
        // active if it is decisively above or below the other branch.
        if (op == Opcode::OP_MAX)
        {
            if (f(a) > f(b))
            {
                return Tape::KEEP_A;
            }
            else if (f(b) > f(a))
            {
                return Tape::KEEP_B;
            }
            else
            {
                return Tape::KEEP_BOTH;
            }
        }
        else if (op == Opcode::OP_MIN)
        {
            if (f(a) > f(b))
            {
                return Tape::KEEP_B;
            }
            else if (f(b) > f(a))
            {
                return Tape::KEEP_A;
            }
            else
            {
                return Tape::KEEP_BOTH;
            }
        }
        return Tape::KEEP_ALWAYS;
    }, Tape::SPECIALIZED);
    return std::make_pair(out, std::move(p));
}

float PointEvaluator::baseEval(const Eigen::Vector3f& pt)
{
    auto handle = tape->getBase(pt); // automatically pops
    return eval(pt);
}

////////////////////////////////////////////////////////////////////////////////

bool PointEvaluator::setVar(Tree::Id var, float value)
{
    auto v = tape->vars.right.find(var);
    if (v != tape->vars.right.end())
    {
        bool changed = f(v->second) != value;
        f.row(v->second) = value;
        return changed;
    }
    else
    {
        return false;
    }
}

////////////////////////////////////////////////////////////////////////////////

void PointEvaluator::operator()(Opcode::Opcode op, Clause::Id id,
                                Clause::Id a_, Clause::Id b_)
{
#define out f(id)
#define a f(a_)
#define b f(b_)
    switch (op)
    {
        case Opcode::OP_ADD:
            out = a + b;
            break;
        case Opcode::OP_MUL:
            out = a * b;
            break;
        case Opcode::OP_MIN:
            out = fmin(a, b);
            break;
        case Opcode::OP_MAX:
            out = fmax(a, b);
            break;
        case Opcode::OP_SUB:
            out = a - b;
            break;
        case Opcode::OP_DIV:
            out = a / b;
            break;
        case Opcode::OP_ATAN2:
            out = atan2(a, b);
            break;
        case Opcode::OP_POW:
            out = pow(a, b);
            break;
        case Opcode::OP_NTH_ROOT:
            // Work around a limitation in pow by using boost's nth-root
            // function on a single-point interval
            if (a < 0)
                out = boost::numeric::nth_root(Interval::I(a, a), b).lower();
            else
                out = pow(a, 1.0f/b);
            break;
        case Opcode::OP_MOD:
            out = std::fmod(a, b);
            while (out < 0)
            {
                out += b;
            }
            break;
        case Opcode::OP_NANFILL:
            out = std::isnan(a) ? b : a;
            break;
        case Opcode::OP_COMPARE:
            if      (a < b)     out = -1;
            else if (a > b)     out =  1;
            else                out =  0;
            break;

        case Opcode::OP_SQUARE:
            out = a * a;
            break;
        case Opcode::OP_SQRT:
            out = sqrt(a);
            break;
        case Opcode::OP_NEG:
            out = -a;
            break;
        case Opcode::OP_SIN:
            out = sin(a);
            break;
        case Opcode::OP_COS:
            out = cos(a);
            break;
        case Opcode::OP_TAN:
            out = tan(a);
            break;
        case Opcode::OP_ASIN:
            out = asin(a);
            break;
        case Opcode::OP_ACOS:
            out = acos(a);
            break;
        case Opcode::OP_ATAN:
            out = atan(a);
            break;
        case Opcode::OP_LOG:
            out = log(a);
            break;
        case Opcode::OP_EXP:
            out = exp(a);
            break;
        case Opcode::OP_ABS:
            out = fabs(a);
            break;
        case Opcode::OP_RECIP:
            out = 1 / a;
            break;

        case Opcode::CONST_VAR:
            out = a;
            break;

        case Opcode::ORACLE:
            tape->oracles[a_]->evalPoint(out);
            break;

        case Opcode::INVALID:
        case Opcode::CONSTANT:
        case Opcode::VAR_X:
        case Opcode::VAR_Y:
        case Opcode::VAR_Z:
        case Opcode::VAR_FREE:
        case Opcode::LAST_OP: assert(false);
    }
#undef out
#undef a
#undef b
}

}   // namespace Kernel


