// Golang scope experiment.
//
// See also "go-scope-1.go".
//
// In this example, an independent 'x' is created as a parameter,
// and the main scope 'x' is copied in as a value, and remains
// unchanged. There is no incorrect synchronization here,
// so "go run -race" should not produce any warnings.

package main

import (
	"log"
	"time"
)

func main() {
	var x int = 1

	go func (x int) {
		for {
			time.Sleep(time.Second)
			x += 1
		}
	}(x)

	for {
		time.Sleep(time.Second)
		log.Printf("x=%d\n", x)
	}
}
