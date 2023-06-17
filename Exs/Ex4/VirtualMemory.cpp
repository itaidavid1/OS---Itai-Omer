#include "VirtualMemory.h"
#include "MemoryConstants.h"
#include "PhysicalMemory.h"
#include <cmath>
#include <cstdio>


#define BIT_MASK 1


int calculate_cyc_diff(uint64_t cur_page_number, uint64_t page_swapped_in){
    if(cur_page_number > page_swapped_in){
        return fmin(NUM_PAGES - cur_page_number - page_swapped_in,
                    cur_page_number - page_swapped_in);

    }
    else{
        return fmin(NUM_PAGES - page_swapped_in - cur_page_number,
                    page_swapped_in - cur_page_number);
    }
}

int check_frame_empty(int frame){ // helper to find if all the table is 0
    uint64_t physical_frame_base_add = frame * PAGE_SIZE;
    int cur_val = 0;
    for (int i = 0; i < PAGE_SIZE; i++){
        PMread(physical_frame_base_add + i, &cur_val);
        if (cur_val != 0){
            return 0;
        }
    }
    return 1;
}


/**
 *
 * @param head_frame the current frame in the tree traversal
 * @param cur_level the curr depth of the tree - stop conditon
 * @param cur_frame the frame we started the dfs check for- not changing during the run
 * @param max_frame_used  the max index frame so far
 * @param max_cyc_diff max_cyc_diff
 * @param page_to_evict page_to_evict
 * @param frame_to_evict frame_to_evict
 * @param parent_frame
 * @param parent_frame_table_idx
 * @param page_swapped_in
 * @param cur_page_number
 * @return
 */
int next_frame_dfs(int head_frame, int cur_level,uint64_t cur_frame, int* max_frame_used,int* max_cyc_diff, uint64_t* page_to_evict,
                   int* frame_to_evict, int * parent_to_evict, int parent_frame, int* frame_ind_in_parent_to_evict,
                   int frame_ind_from_parent, uint64_t page_swapped_in, uint64_t cur_page_number ){

    if (head_frame > *max_frame_used){ // update the max frame use index
        *(max_frame_used) = head_frame;
    }

    if (cur_level == TABLES_DEPTH){ // validation of finish dfs until the depth
        // todo : calculate evicted pages
        // cur page - need to calculate the tree traversal
        //calculate
        int cur_cyc_dist = calculate_cyc_diff(cur_page_number ,page_swapped_in);

        if(cur_cyc_dist > *(max_cyc_diff) && cur_frame != head_frame)
        {
            *(max_cyc_diff) = cur_cyc_dist;
            *(frame_to_evict) = head_frame;
            *(page_to_evict) = cur_page_number;
            *(parent_to_evict) = parent_frame;
            *(frame_ind_in_parent_to_evict) = frame_ind_from_parent;
        }

        return 0;
    }

    // todo- another option to check 2 times if the frame is empty
//    int is_frame_empty = check_frame_empty(head_frame); // find if curr table node is empty
//    if (is_frame_empty && head_frame != cur_frame){ // found a frame and return it
//        return head_frame;
//    }
    //

    bool is_frame_empty = true;
    for (int i = 0; i < PAGE_SIZE; i++){
        int child_idx = 0;
        PMread((head_frame * PAGE_SIZE) + i, &child_idx);
        if(child_idx !=0 ){
            is_frame_empty = false;
//              cur_page_number = (cur_page_number << OFFSET_WIDTH) + i; // todo to the bfs
            int ret_frame = next_frame_dfs(child_idx, cur_level + 1,cur_frame, max_frame_used,max_cyc_diff,page_to_evict,
                                           frame_to_evict,parent_to_evict,head_frame, frame_ind_in_parent_to_evict,
                                           i,page_swapped_in,(cur_page_number << OFFSET_WIDTH) + i);
            if(ret_frame != 0){
                if (ret_frame == child_idx){
                    *(parent_to_evict) = head_frame;
                    *(frame_ind_in_parent_to_evict) = i;
                }
                return ret_frame;
            }

        }
    }

    if (is_frame_empty && head_frame != cur_frame){ // found a frame and return it
        return head_frame;
    }

    return 0;

}


int find_next_available_frame(uint64_t cur_frame, uint64_t page_swapped_in){
    // option 1

    // option 2
    int max_frame_used = 0;
    int max_frame_base_address = 0;

    // option 3
    int max_cyc_diff = 0;
    uint64_t page_to_evict = 0;
    int frame_to_evict = 0;
    int parent_to_evict = 0;

    int frame_ind_in_parent_to_evict = 0; // which index to delete
    int frame_to_evict_base_address = 0;

    int next_available_frame = next_frame_dfs(0, 0, cur_frame,  &max_frame_used, &max_cyc_diff,
                                              &page_to_evict,
                                              &frame_to_evict,
                                              &parent_to_evict,
                                              0,
                                              &frame_ind_in_parent_to_evict,0, page_swapped_in,
                                              0); // todo fix dfs argments
    if(next_available_frame != 0){
        PMwrite(parent_to_evict*PAGE_SIZE  + frame_ind_in_parent_to_evict, 0);
        return next_available_frame; // first option - find if there is a frame with all 0
    }
    if(max_frame_used + 1 < NUM_FRAMES){
        max_frame_base_address = (max_frame_used + 1 ) * PAGE_SIZE;
        for(int i = 0; i< PAGE_SIZE; i++){
            PMwrite(max_frame_base_address + i, 0); // all 0 in the new frame
        }
        return max_frame_used + 1;
    }

    // cycle
    PMevict(frame_to_evict, page_to_evict);
    for(int i = 0; i< PAGE_SIZE; i++){
        frame_to_evict_base_address = (frame_to_evict ) * PAGE_SIZE;
        PMwrite(frame_to_evict_base_address + i, 0); // all 0 in the new frame
    }
    // disconnect the parent of the current table
    // we get the
    PMwrite(parent_to_evict * PAGE_SIZE + frame_ind_in_parent_to_evict, 0);


    return frame_to_evict;

    // should check


}


uint64_t get_table_bits(uint64_t address, int tree_level){
    uint64_t cur_add = address >> OFFSET_WIDTH; // drop the offset
    cur_add = cur_add >> ((TABLES_DEPTH - tree_level -1 ) * OFFSET_WIDTH); // shift bits
    return cur_add & (PAGE_SIZE - 1);
}

/**
 *
 * @param virtual_address
 * @return
 */
uint64_t convert_virtual2physical(uint64_t virtual_address){
    uint64_t cur_frame = 0;
    int next_frame = 0;
    uint64_t restored_page_index = virtual_address >> OFFSET_WIDTH; // calculate the page index for restoring from the disk
    uint64_t offset_address = virtual_address & (PAGE_SIZE - 1); // calculate offset of the word in the page

    for (int d = 0; d < TABLES_DEPTH; d++){
        uint64_t cur_vir_bits = get_table_bits(virtual_address, d);
        PMread(cur_frame * PAGE_SIZE + cur_vir_bits, &next_frame);

        if (next_frame == 0){
            // find available frame
            next_frame = find_next_available_frame(cur_frame, restored_page_index);

            if (next_frame == 0){ // didn't find an empty frame in the RAM
                return 0; // page fault
            }
            else {
                PMwrite(cur_frame * PAGE_SIZE + cur_vir_bits, next_frame);
            }

        }
        cur_frame = next_frame;
    }

    PMrestore(cur_frame, restored_page_index);
    return cur_frame * PAGE_SIZE + offset_address; // return the address to write/read the word from the physical address


}


/*
 * Initialize the virtual memory.
 */
void VMinitialize(){
    for (uint64_t i = 0; i < PAGE_SIZE; ++i){
        PMwrite(i, 0);
    }
}
/* Reads a word from the given virtual address
 * and puts its content in *value.
 *
 * returns 1 on success.
 * returns 0 on failure (if the address cannot be mapped to a physical
 * address for any reason)
 */
int VMread(uint64_t virtualAddress, word_t* value){
    if (virtualAddress > VIRTUAL_MEMORY_SIZE - 1){
        return 0;
    }
    // convert virtual to physical address
    uint64_t physical_address_to_read = convert_virtual2physical(virtualAddress);
//    if(physical_address_to_read == 0){
//        return 0; // todo verify if we need to deal with page fault or address 0
//    }
    PMread(physical_address_to_read, value);
    return 1;
}

/* Writes a word to the given virtual address.
 *
 * returns 1 on success.
 * returns 0 on failure (if the address cannot be mapped to a physical
 * address for any reason)
 */
int VMwrite(uint64_t virtualAddress, word_t value){
    if (virtualAddress > VIRTUAL_MEMORY_SIZE - 1){
        return 0;
    }
    uint64_t physical_address_to_read = convert_virtual2physical(virtualAddress);
//    if(physical_address_to_read == 0){
//        return 0; // todo verify if we need to deal with page fault or address 0
//    }
    printf("address value %llu %d\n", (long long int)physical_address_to_read , value);
    PMwrite(physical_address_to_read, value);
    return 1;
}
