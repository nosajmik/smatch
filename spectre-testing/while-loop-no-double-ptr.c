#include <stdio.h>
#include <stdlib.h>

// Define the structure of each node
typedef struct Node {
    int *data;
    struct Node *next; // Pointer to the next node
} Node;

// Function to create a new node
Node* createNode(int *data) {
    Node *newNode = (Node*)malloc(sizeof(Node));
    if (newNode == NULL) {
        printf("Memory allocation failed\n");
        exit(1);
    }
    newNode->data = data;
    newNode->next = NULL;
    return newNode;
}

// Function to traverse the linked list and print each data element
void traverseList(Node *head) {
    Node *current = head;
    while (current != NULL) {
        volatile int temp = *(current->data);
        current = current->next;
    }
}

int main(int argc, char **argv) {
    // Create some sample data
    int data1 = 3.14;
    int data2 = 6.28;

    // Create pointers to the data
    int *ptr1 = &data1;
    int *ptr2 = &data2;

    // Create nodes with these double pointers
    Node *node1 = createNode(ptr1);
    Node *node2 = createNode(ptr2);

    // Link the nodes
    node1->next = node2;

    // Traverse and print the linked list
    traverseList(node1);

    // Free the allocated memory
    free(node1);
    free(node2);

    return 0;
}
