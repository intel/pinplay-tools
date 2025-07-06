/*
* Copyright (C) 2024-2024 Intel Corporation
*
* SPDX-License-Identifier: BSD-3-Clause
*/
#include <buffer_pool.h>

namespace bufferpool
{
    
    /*
    * Buffer_pool
    */
    Buffer_pool::Buffer_pool(uint16_t poolSize, size_t expansionSize = EXPANSION_SIZE, size_t maxPoolSize = MAX_POOL_SIZE) : 
        bufferPoolSize_(poolSize),
        expansionSize_(expansionSize),
        maxPoolSize_(maxPoolSize),
        currentAllocatedBuffersNum_(0)
    {
        // Just save the size of the pool but don't allocate it. Explanation in create().
    }

    /*
    * create
    */
    Buffer_pool::Buffer_pool_ptr Buffer_pool::create(uint16_t poolSize)
    {
        Buffer_pool_ptr ret;
        auto pool = new (std::nothrow) Buffer_pool(poolSize);
        if(nullptr == pool)
        {
            return nullptr;
        }
        ret.reset(pool);
        pool->this_ = ret;
        // We don't call allocate_pool from the above constructor because of circular dependency.
        // Allocating a buffer also allocates Buffer_deleter that needs a valid weak_ptr to Buffer_pool
        // which is stored in pool->this_ . Let the Buffer_pool get fully created and then intialize it.
        pool->allocate_pool();
        return ret;
    }

    /*
    * adds new buffers to the pull
    */
   void Buffer_pool::expand_pool ()
   {
        allocate_additional_buffers(expansionSize_);
   }


    /*
    * allocate_pool
    */
    void Buffer_pool::allocate_pool()
    {
        allocate_additional_buffers(bufferPoolSize_);
    }

    /*
    * allocate additional buffers
    */
    void Buffer_pool::allocate_additional_buffers(uint16_t count)
    {
        std::lock_guard< std::mutex > guard(poolMutex_);
        for (auto i = 0; i < count; i++)
        {
            auto buf = new (std::nothrow) Buffer();
            if(nullptr == buf)
            {
                continue;
            } 
            add_buffer_internal(buf);
            currentAllocatedBuffersNum_++;
        }
    }

    /*
    * add_buffer
    */
    void Buffer_pool::add_buffer(Buffer* buffer)
    {
        std::lock_guard< std::mutex > guard(poolMutex_);
        add_buffer_internal(buffer);
    }

    /*
    * add_buffer internal
    */
    void Buffer_pool::add_buffer_internal(Buffer* buffer)
    {
        if (nullptr != buffer)
        {
            pool_.push(Buffer_pool::Buffer_ptr(buffer, Buffer_deleter {this_}));
        }
    }

    /*
    * get_buffer
    */
    Buffer_pool::Buffer_ptr Buffer_pool::get_buffer()
    {
        if (pool_.empty())
        {      
            //Check if can expand the pool
            if(currentAllocatedBuffersNum_ + expansionSize_ > maxPoolSize_)   
            {
                return Buffer_pool::get_null_buffer();
            }   
            expand_pool();
        }

        {
            std::lock_guard< std::mutex > guard(poolMutex_);
            auto element = std::move(pool_.top());
            pool_.pop();
            return element;
        }
        
    }

    /*
    * get_null_buffer
    */
    Buffer_pool::Buffer_ptr Buffer_pool::get_null_buffer() { return Buffer_ptr(nullptr, Buffer_deleter {this_}); }

};
