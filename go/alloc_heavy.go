package main
import (
	"fmt"
	"strconv"
	"strings"
	"time"
)

func main() {
	start := time.Now()
	var items []string
	for i := 0; i < 750000; i++ {
		items = append(items, strings.Repeat("item-", 2) + strconv.Itoa(i))
	}
	fmt.Println("alloc_heavy", len(items))
	fmt.Printf("Time: %v\n", time.Since(start))
}