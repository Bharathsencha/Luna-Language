package main
import (
	"fmt"
	"strconv"
	"strings"
	"time"
)

func main() {
	start := time.Now()
	var table []string
	for i := 0; i < 200000; i++ {
		table = append(table, strings.Repeat("value-", 2) + strconv.Itoa(i))
	}
	for j := 0; j < 400000; j++ {
		_ = strings.Repeat("scratch-", 2) + strconv.Itoa(j)
	}
	fmt.Println("long_live", len(table))
	fmt.Printf("Time: %v\n", time.Since(start))
}