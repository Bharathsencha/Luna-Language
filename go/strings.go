package main

import (
	"fmt"
	"strconv"
	"strings"
	"time"
)

func main() {
	start := time.Now()
	var out []string
	for i := 0; i < 500000; i++ {
		s := strings.Repeat("luna", 16) + "-" + strconv.Itoa(i)
		out = append(out, s)
	}
	
	fmt.Println("strings", len(out))
	fmt.Printf("Time: %v\n", time.Since(start))
}