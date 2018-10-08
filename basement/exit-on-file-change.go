// Saving this for later reference.

func exit_on_file_change() {
	t1 := time.Unix(0,0)
	for {
		st, err := os.Stat("instaskunk.go")
		if err == nil {
			t2 := st.ModTime()
			if t1 != time.Unix(0,0) && t1 != t2 {
				fmt.Printf("restart!\n")
				os.Exit(0);
			}
			t1 = t2
		}
		time.Sleep(time.Second)
	}
}

// go exit_on_file_change()
