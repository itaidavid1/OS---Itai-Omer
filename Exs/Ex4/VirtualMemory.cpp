#include "VirtualMemory.h"
#include "MemoryConstants.h"
#include "PhysicalMemory.h"




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

int next_frame_dfs(int head_frame, int* max_frame_used, int cur_level){
    if (cur_level == TABLES_DEPTH){ // validation of finish dfs until the depth
        return 0;
    }
    int is_frame_empty = check_frame_empty(head_frame); // find if curr table node is empty
    if (is_frame_empty){ // found a frame and return it
        return head_frame;
    }
    if (head_frame > *max_frame_used){ // update the max frame use index
        *max_frame_used = head_frame;
    }
    //
    for (int i = 0; i < PAGE_SIZE; i++){
        int child_idx = 0;
        PMread((head_frame * PAGE_SIZE) + i, &child_idx);
        if (next_frame_dfs(child_idx, max_frame_used, cur_level + 1) != 0){
            return child_idx;
        }
    }

    return 0;



}


int find_next_available_frame(){

    int max_frame_used = 0;
    int max_cyc_diff = 0;
    int page_to_evict = 0;
    int frame_to_evict = 0;

    int frame_base_address = 0;

    int next_available_frame = next_frame_dfs(&max_frame_used, &max_cyc_diff, &page_to_evict, &frame_to_evict); // todo fix dfs argments
    if(next_available_frame != 0){
        return next_available_frame; // first option - find if there is a frame with all 0
    }
    if(max_frame_used + 1 < NUM_FRAMES){
        frame_base_address = (max_frame_used + 1 ) * PAGE_SIZE;
        for(int i = 0; i< NUM_PAGES; i++){
            PMwrite(frame_base_address + i, 0); // all 0 in the new frame
        }
        return max_frame_used + 1;
    }

    // cycle
    PMevict(frame_to_evict, page_to_evict);
    for(int i = 0; i< NUM_PAGES; i++){
        frame_base_address = (frame_to_evict ) * PAGE_SIZE;
        PMwrite(frame_base_address + i, 0); // all 0 in the new frame
    }
    // todo : disconnect the parent of the current table


    return frame_to_evict;



}


uint64_t get_table_bits(uint64_t address, int tree_level){
    uint64_t cur_add = address >> OFFSET_WIDTH; // drop the offset
    cur_add = cur_add >> ((TABLES_DEPTH - tree_level - 1) * OFFSET_WIDTH); // shift bits
    return cur_add & OFFSET_WIDTH;
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
            next_frame = find_next_available_frame();

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
    PMwrite(physical_address_to_read, value);
}
