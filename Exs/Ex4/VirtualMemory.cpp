#include "VirtualMemory.h"
#include "PhysicalMemory.h"
#include <cmath>

/**
 * function which calculates the cyclic distance between 2 given page
 * @param cur_page_number the page we want to evict
 * @param page_swapped_in  the page we want to put in
 * @return the cyclic distance
 */
int calculate_cyc_diff(uint64_t cur_page_number, uint64_t page_swapped_in){
    if(cur_page_number > page_swapped_in){
        return fmin(NUM_PAGES - (cur_page_number - page_swapped_in),
                    cur_page_number - page_swapped_in);

    }
    else{
        return fmin(NUM_PAGES - (page_swapped_in - cur_page_number),
                    page_swapped_in - cur_page_number);
    }
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
 * @param parent_frame parent frame in the tree of the returned frame
 * @param parent_frame_table_idx index in the parent frame which maps the returned frame
 * @param page_swapped_in page we want to put in the physical memory
 * @param cur_page_number the current binary representation of the frame we stand on in the tree traversal
 * @return frame idx if succeeded 0 if failed
 */
int next_frame_dfs(int head_frame, int cur_level,uint64_t cur_frame, int* max_frame_used,int* max_cyc_diff, uint64_t* page_to_evict,
                   int* frame_to_evict, int * parent_to_evict, int parent_frame, int* frame_ind_in_parent_to_evict,
                   int frame_ind_from_parent, uint64_t page_swapped_in, uint64_t cur_page_number ){

    if (head_frame > *max_frame_used){ // update the max frame use index
        *(max_frame_used) = head_frame;
    }

    if (cur_level == TABLES_DEPTH){ // validation of finish dfs until the depth

        int cur_cyc_dist = calculate_cyc_diff(cur_page_number ,page_swapped_in);

        if(cur_cyc_dist > *(max_cyc_diff) && cur_frame != static_cast<uint64_t>(head_frame))
        {
            *(max_cyc_diff) = cur_cyc_dist;
            *(frame_to_evict) = head_frame;
            *(page_to_evict) = cur_page_number;
            *(parent_to_evict) = parent_frame;
            *(frame_ind_in_parent_to_evict) = frame_ind_from_parent;
        }

        return 0;
    }

    bool is_frame_empty = true;
    for (int i = 0; i < PAGE_SIZE; i++){
        int child_idx = 0;
        PMread((head_frame * PAGE_SIZE) + i, &child_idx);
        if(child_idx !=0 ){
            is_frame_empty = false;
            int ret_frame = next_frame_dfs(child_idx, cur_level + 1,cur_frame, max_frame_used,max_cyc_diff,page_to_evict,
                                           frame_to_evict,parent_to_evict,head_frame, frame_ind_in_parent_to_evict,
                                           i,page_swapped_in,(cur_page_number << OFFSET_WIDTH) + i);
            if(ret_frame != 0){
                return ret_frame;
            }
        }
    }

    if (is_frame_empty &&  static_cast<uint64_t>(head_frame) !=  cur_frame){ // found a frame and return it
        *(parent_to_evict) = parent_frame;
        *(frame_ind_in_parent_to_evict) = frame_ind_from_parent;
        return head_frame;
    }

    return 0;

}

/**
 * function which return the frame we will enter it the next page
 * @param cur_frame the frame we want to enter the next frame mapping into
 * @param page_swapped_in  the page we want to put in the physical memory
 * @param isPage flag to distinct if we are looking for frame for page
 * @return the frame we want to put into
 */
int find_next_available_frame(uint64_t cur_frame, uint64_t page_swapped_in, bool isPage){
    // option 1

    // option 2
    int max_frame_used = 0;
    int max_frame_base_address;

    // option 3
    int max_cyc_diff = 0;
    uint64_t page_to_evict = 0;
    int frame_to_evict = 0;
    int parent_to_evict = 0;

    int frame_ind_in_parent_to_evict = 0; // which index to delete
    int frame_to_evict_base_address;

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
            if(! isPage)
            {
                PMwrite(max_frame_base_address + i, 0); // all 0 in the new frame
            }
        }
        return max_frame_used + 1;
    }

    // cycle
    PMevict(frame_to_evict, page_to_evict);
    for(int i = 0; i< PAGE_SIZE; i++){
        frame_to_evict_base_address = (frame_to_evict ) * PAGE_SIZE;
        if(! isPage)
        {
            PMwrite(frame_to_evict_base_address + i, 0); // all 0 in the new frame
        }
    }
    // disconnect the parent of the current table
    PMwrite(parent_to_evict * PAGE_SIZE + frame_ind_in_parent_to_evict, 0);

    return frame_to_evict;

}

/**
 * returns the relevant bits for the current map, depends on the current height in the tree
 * @param address the full virtual address
 * @param tree_level the current level in the tree traversal
 * @return those relevant bits
 */
uint64_t get_table_bits(uint64_t address, int tree_level){
    uint64_t cur_add = address >> OFFSET_WIDTH; // drop the offset
    cur_add = cur_add >> ((TABLES_DEPTH - tree_level -1 ) * OFFSET_WIDTH); // shift bits
    return cur_add & (PAGE_SIZE - 1);
}

/**
 * responsible for converting virtual address to physical address
 * @param virtual_address virtual_address
 * @returnthe physical address
 */
uint64_t convert_virtual2physical(uint64_t virtual_address){
    uint64_t cur_frame = 0;
    int next_frame = 0;
    uint64_t restored_page_index = virtual_address >> OFFSET_WIDTH; // calculate the page index for restoring from the disk
    uint64_t offset_address = virtual_address & (PAGE_SIZE - 1); // calculate offset of the word in the page

    for (int d = 0; d < TABLES_DEPTH; d++){
        bool isPage = false;
        if(d ==  TABLES_DEPTH -1){
            isPage = true;
        }
        uint64_t cur_vir_bits = get_table_bits(virtual_address, d);
        PMread(cur_frame * PAGE_SIZE + cur_vir_bits, &next_frame);

        if (next_frame == 0){
            // find available frame
            next_frame = find_next_available_frame(cur_frame, restored_page_index, isPage);

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
    if(physical_address_to_read == 0 && TABLES_DEPTH > 0 ){
        return 0;
    }
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
    if(physical_address_to_read == 0 && TABLES_DEPTH > 0 ){
        return 0;
    }
    PMwrite(physical_address_to_read, value);
    return 1;
}
