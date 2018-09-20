// Replace tabs with spaces in named files.
// Suggested use pattern:
//   * Start with tracked files under version control
//   * Run ths program
//   * Diff versus commited version
//   * If Ok, commit new version.
//
package main

import (
	"fmt"
	//"path/filepath"
        "io/ioutil"
	"os"
	"bufio"
	"bytes"
)

func untabify(line string) string {
	result := ""
	t := 8 // tab stop distance
	for _, v := range line {
		c := string(v)
		if c == "\t" {
			count := t
			for s := 0; s < count; s++ {
				result += " "
				t -= 1
			}
		} else {
			result += c
			t -= 1
		}
		if (t == 0) {
			t = 8
		}
	}
	return result
}

func main() {
	files := os.Args[1:]
	for i:=0; i < len(files); i++ {
		data1, _ := ioutil.ReadFile(files[i])
		data2 := ""
		file, err := os.Open(files[i])
		if err != nil {
			continue
		}
		defer file.Close()
		scanner := bufio.NewScanner(file)
		for scanner.Scan() {
			line := scanner.Text()
			data2 = data2 + untabify(line) + "\n"
		}
		if scanner.Err() == nil {
			// No errors.
			if bytes.Compare(data1, []byte(data2)) == 0 {
				fmt.Printf("%s Ok\n", files[i])
			} else {
				ioutil.WriteFile(files[i], []byte(data2), 0664)
				fmt.Printf("%s modified\n", files[i])
			}
		}
	}
}
