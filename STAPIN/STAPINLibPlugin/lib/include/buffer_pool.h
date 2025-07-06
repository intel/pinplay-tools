/*
* Copyright (C) 2024-2024 Intel Corporation
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef _BUFFER_POOL_H_
#define _BUFFER_POOL_H_

#include <stdint.h>

#include <stack>
#include <memory>
#include <mutex>
#include <atomic>

namespace bufferpool
{
    inline constexpr uint16_t BUFFER_SIZE = 13000; 
    inline constexpr size_t EXPANSION_SIZE = 24;
    inline constexpr size_t MAX_POOL_SIZE = 240;

    /**
     * @brief A class that is a simple wrapper for a static byte array
     * 
     */
    struct Buffer
    {
        uint8_t data[BUFFER_SIZE];
    };

    /**
     * @brief   An interface just for adding a buffer.
     *          The Buffer_pool class will implement this interface and add more API functions.
     *          This interface is required to break the circular dependency between 
     *          Buffer_pool and Buffer_deleter (forward declarations don't work well with all compilers).
     *          This interface is what the Buffer_deleter needs to recycle a buffer.
     */
    struct IBuffer_pool
    {
        virtual void add_buffer(Buffer* buffer) = 0;
    };

    /**
     * @brief   A class that recycles buffers back into the buffer pool.
     *          The class is initialized with a pointer to the buffer pool.
     *          It has a delete operator for a Buffer instance, where the buffer
     *          is pushed back to the pool.
     * 
     */
    struct Buffer_deleter
    {
        std::weak_ptr< IBuffer_pool > bufferPool_;
        Buffer_deleter() = delete;
        explicit Buffer_deleter(std::weak_ptr< IBuffer_pool >& pool) : bufferPool_(pool) {}
        void operator()(Buffer* p)
        {
            auto pool = bufferPool_.lock();
            if (pool)
            {
                pool->add_buffer(p);
            }
            else
            {
                delete p;
            }
        }
    };

    /**
     * @brief   A class for allocating buffers from a pool.
     *          The class maintains a pool of buffers in a stack.
     *          When allocating a buffer it returns a unique_ptr with a custom deleter.
     *          When the object is released, the custom deleter is activated and recycles
     *          the buffer back to the pool.
     */
    class Buffer_pool : public IBuffer_pool
    {
        Buffer_pool(Buffer_pool&)  = delete;
        Buffer_pool(Buffer_pool&&) = delete;
        Buffer_pool& operator=(Buffer_pool&) = delete;
        Buffer_pool& operator=(Buffer_pool&&) = delete;

    public:
        using Buffer_ptr = std::unique_ptr< Buffer, Buffer_deleter >;
        using Buffer_pool_ptr = std::shared_ptr< Buffer_pool >;

        Buffer_pool() = delete;
        ~Buffer_pool() = default;

        /**
         * @brief Create the buffer pool
         * 
         * @param poolSize              The number of buffers in the pool.
         *                              The size of the buffer is fixed to BUFFER_SIZE,
         * @return Buffer_pool_ptr      A shared_ptr to the buffer pool
         */
        static Buffer_pool_ptr create(uint16_t poolSize);

        /**
         * @brief Push a buffer to the pool
         * 
         * @param buffer The buffer object
         */
        void add_buffer(Buffer* buffer) override;

        /**
         * @brief Get a buffer from the pool
         * 
         * @return If the pool has available buffers, returns a unique_ptr that owns the buffer, with a custom deleter.
         *         If the pool is empty it returns an empty unique_ptr.
         */
        Buffer_ptr get_buffer();

        /**
         * @brief Return an empty buffer. 
         *        This function is required to initialize a buffer pointer to point to nothing.
         *        We cannot just initialize a unique_ptr to nullptr since the object also needs 
         *        to include an instance of the buffer deleter.
         * 
         * @return A unique_ptr initialized to nullptr
         */
        Buffer_ptr get_null_buffer();

        /**
         * @brief expands the pool by expansionSize_.
         */
        void expand_pool ();

    private:
        explicit Buffer_pool(uint16_t poolSize, size_t expansionSize, size_t maxPoolSize);
        void allocate_pool();
        void allocate_additional_buffers(uint16_t count);
        void add_buffer_internal(Buffer* buffer);
        std::weak_ptr< IBuffer_pool > this_;
        std::stack< Buffer_ptr > pool_;
        std::mutex poolMutex_;
        uint16_t bufferPoolSize_;
        size_t expansionSize_;
        size_t maxPoolSize_;
        std::atomic_uint16_t currentAllocatedBuffersNum_;
    };

} // namespace bufferpool

#endif // _BUFFER_POOL_H_
