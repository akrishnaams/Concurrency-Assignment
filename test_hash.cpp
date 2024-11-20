#include <iostream>
#include <cassert>
#include <string>
#include <vector>
#include "hash.cpp"  // Assuming your class is in this file

void testInsertAndRead() {
    HashTable hashTable(10);
    
    // Test insertion
    hashTable.insert("apple");
    hashTable.insert("banana");
    
    // Test that the values can be read correctly
    assert(hashTable.read("apple") == true);  // Should find apple
    assert(hashTable.read("banana") == true); // Should find banana
    assert(hashTable.read("cherry") == false); // Should not find cherry
}

void testRemove() {
    HashTable hashTable(10);
    
    // Test insertion
    hashTable.insert("apple");
    hashTable.insert("banana");
    
    // Test removal
    hashTable.remove("apple");
    assert(hashTable.read("apple") == false); // apple should be removed
    assert(hashTable.read("banana") == true); // banana should still be present
    
    hashTable.remove("banana");
    assert(hashTable.read("banana") == false); // banana should also be removed
}

void testInsertRemoveWithCollision() {
    HashTable hashTable(3); // Using a smaller table size to force collisions
    
    // Inserting elements that should collide due to small table size
    hashTable.insert("a");
    hashTable.insert("b");
    hashTable.insert("c");
    
    // Test that all elements are in the hash table
    assert(hashTable.read("a") == true);
    assert(hashTable.read("b") == true);
    assert(hashTable.read("c") == true);
    
    // Remove one element and check the others
    hashTable.remove("b");
    assert(hashTable.read("a") == true);
    assert(hashTable.read("b") == false);
    assert(hashTable.read("c") == true);
}

void testEmptyTable() {
    HashTable hashTable(5);
    
    // Test on an empty table
    assert(hashTable.read("apple") == false);  // Should not find apple
    hashTable.remove("apple");  // Should not crash or do anything
}

int main() {
    std::cout << "Running tests...\n";
    
    testInsertAndRead();
    std::cout << "Insert and Read test passed.\n";
    
    testRemove();
    std::cout << "Remove test passed.\n";
    
    testInsertRemoveWithCollision();
    std::cout << "Insert, Remove with Collision test passed.\n";
    
    testEmptyTable();
    std::cout << "Empty Table test passed.\n";
    
    std::cout << "All tests passed.\n";
    
    return 0;
}
