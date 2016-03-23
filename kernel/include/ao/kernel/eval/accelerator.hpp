/*
 *  Copyright (C) 2016 Matthew Keeter  <matt.j.keeter@gmail.com>
 *
 *  This file is part of the Ao library.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  Ao is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with Ao.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#include <unordered_map>
#include <array>

class Evaluator;
class Clause;

class Accelerator
{
public:
    Accelerator(Evaluator* e);
    ~Accelerator();

    /*
     *  Evaluates a certain number of values, returning a
     *  pointer to device memory.
     */
    float* values(size_t count);

    /*
     *  Loads data from the device to host memory, returning a pointer
     *  (which is owned by the Accelerator)
     */
    float* fromDevice(float* ptr_d);

    /*
     *  Sets a value in host memory
     *  (inlined for speed)
     *
     *  In practice, you should use a more efficient GPU-side strategy
     *  to load values (e.g. a kernel that unwraps a region and assigns
     *  appropriate positions)
     */
    void set(float x, float y, float z, size_t index)
    {
        X[index] = x;
        Y[index] = y;
        Z[index] = z;
    }

    /*
     *  Writes X, Y, Z arrays to device memory
     */
    void toDevice();

    /*  Samples per clause  */
    static constexpr size_t N=4096;
protected:
    /*
     *  Allocates device memory for the given clause
     *  and stores it in the mem array
     */
    void allocate(Clause* c);

    /*  Memory allocated on the target device  */
    std::unordered_map<Clause*, float*> mem;

    /*  Buffer used when copying to and from device memory  */
    std::array<float, N> buf;
    std::array<float, N> X;
    std::array<float, N> Y;
    std::array<float, N> Z;

    /*  Pointer back to parent evaluator  */
    const Evaluator* const evaluator;
};
