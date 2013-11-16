#ifndef LINUX_QUEUE_QUEUE_HXX
#define LINUX_QUEUE_QUEUE_HXX

#include <cstdint>
#include <cstring>

#ifdef DEBUG
#include <cstdio>
#endif

namespace linux {

    namespace queue {

        // ONLY support x86_64 platform!
        // On x86_64, only store-load may be reordered, following
        // will never be reordered:
        // store-store
        // load-load
        // load-store

        /****************************************************************
         ** single reader and single writer
         ***************************************************************/
        template<typename T>
        class sr_sw_queue {
        public:
            sr_sw_queue(): reader_pos(0), writer_pos(0) {
                std::memset(circular_buffer, 0, sizeof(circular_buffer));
            }
            bool add(T* p) {
                if(count() == buffer_size) {
                    return false;
                }
                auto write_index = writer_pos & (buffer_size - 1);
                circular_buffer[write_index] = p;
                // should insert compiler barrier here on x86_64
                __asm__ __volatile__ ("" ::: "memory");
                ++writer_pos;
                return true;
            }
            bool remove(T*& p) {
                if(count() == 0) {
                    return false;
                }
                auto read_index = reader_pos & (buffer_size - 1);
                p = circular_buffer[read_index];
                // should insert compiler barrier here on x86_64
                __asm__ __volatile__ ("" ::: "memory");
                ++reader_pos;
                return true;
            }
        private:
            std::uint64_t count() {
                return writer_pos - reader_pos;
            }
            std::uint64_t reader_pos;
            std::uint64_t writer_pos;
            constexpr static const std::uint64_t buffer_size = 65536;
            T* circular_buffer[buffer_size];
        };

        /****************************************************************
         ** single reader and multiple writers
         ***************************************************************/
        template<typename T>
        class sr_mw_queue {
        public:
            sr_mw_queue(): reader_pos(0), writer_pos(0), writer_lock(0) {
                std::memset(circular_buffer, 0, sizeof(circular_buffer));
            }
            bool add(T* p) {
                while(__sync_val_compare_and_swap(&writer_lock, 0, 1) != 0);
                if(count() == buffer_size) {
                    writer_lock = 0;
                    return false;
                }
                auto write_index = writer_pos & (buffer_size - 1);
                circular_buffer[write_index] = p;
                // should insert compiler barrier here on x86_64
                __asm__ __volatile__ ("" ::: "memory");
                ++writer_pos;
                __asm__ __volatile__ ("" ::: "memory");
                writer_lock = 0;
                return true;
            }
            bool remove(T*& p) {
                if(count() == 0) {
                    return false;
                }
                auto read_index = reader_pos & (buffer_size - 1);
                p = circular_buffer[read_index];
                // should insert compiler barrier here on x86_64
                __asm__ __volatile__ ("" ::: "memory");
                ++reader_pos;
                return true;
            }
        private:
            std::uint64_t count() {
                return writer_pos - reader_pos;
            }
            std::uint64_t reader_pos;
            std::uint64_t writer_pos;
            std::uint64_t writer_lock;
            constexpr static const std::uint64_t buffer_size = 65536;
            T* circular_buffer[buffer_size];
        };
    }
}

#endif
