package main

import (
	"fmt"
	"strconv"
	"time"
)

type Node struct {
	Name string
	Next *Node
}

func main() {
	start := time.Now()
	nodes := make([]*Node, 0, 50000)
	for i := 0; i < 50000; i++ {
		nodes = append(nodes, &Node{Name: "n" + strconv.Itoa(i)})
	}

	for i := 0; i < len(nodes); i++ {
		nextI := (i + 1) % len(nodes)
		nodes[i].Next = nodes[nextI]
	}

	fmt.Println("cycles", len(nodes))
	fmt.Printf("Time: %v\n", time.Since(start))
}