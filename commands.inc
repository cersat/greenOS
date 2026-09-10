cmd_ret cmd_cls(cmd_args args) {
	(void)args;
	clear_screen();
	cmd_ret ret;
	ret.errorcode = 0;
	return ret;
}

cmd_ret cmd_color(cmd_args args) {
	cmd_ret ret;
	ret.errorcode = 0;
	wsf("d", args.amount);
	if(args.amount < 1) {
        RET_ERRCODE(3, "Insufficient arguments");
	}
    int prev_color = VGA_COLOR;
	VGA_COLOR = (str_to_hex(args.args[0][0]) << 4) | str_to_hex(args.args[0][1]);
    if(!VGA_COLOR) {
        VGA_COLOR = prev_color;
        ret.errorcode = 1;
        strcat(ret.error, "Incorrect color");
    }
	return ret;
}

cmd_ret cmd_echo(cmd_args args) {
    put_char('\n');
    write_string(args.args[0]);
	cmd_ret ret;
	ret.errorcode = 0;
	return ret;
}

cmd_ret cmd_delay(cmd_args args) {
	cmd_ret ret;
	ret.errorcode = 0;
	if(args.amount < 1) {
        RET_ERRCODE(3, "Insufficient arguments");
	}
    int del = str_to_int(args.args[0]);
    if(del <= 0 || del >= 100000) {
		RET_ERRCODE(1, "Invalid delay");
    }
    else delay(del);
	return ret;
}

cmd_ret cmd_run(cmd_args args) {
	(void)args;
	cmd_ret ret;
	ret.errorcode = 0;
    if (read_select == 0) { 
		RET_ERRCODE(2, "No file selected");
    }
    run_script((u32)read_select);
	return ret;
}

cmd_ret cmd_dumb(cmd_args args) {
    cmd_ret ret;
    ret.errorcode = 0;
	if(args.amount < 1) {
        RET_ERRCODE(3, "Insufficient arguments");
	}
    if (read_select == 0) {
		RET_ERRCODE(2, "No file selected");
    }
    put_char('\n');
    readFilePart(read_select, str_to_int(args.args[0]), sector_buffer);

    for (int i = 0; i < 512; i += 16) {
        wsf("dc ", i, ':');

        for (int j = 0; j < 16; j++) {
            u8 b = sector_buffer[i + j];
            char hex[3] = {0};
            hex[0] = (b >> 4) < 10 ? '0' + (b >> 4) : 'A' + ((b >> 4) - 10);
            hex[1] = (b & 0x0F) < 10 ? '0' + (b & 0x0F) : 'A' + ((b & 0x0F) - 10);
            hex[2] = '\0';
            wsf("s ", hex);
        }

        write_string(" |");
        for (int j = 0; j < 16; j++) {
            u8 c = sector_buffer[i + j];
            if (c > 10) put_char(c);
            else put_char_color('.', 0x70);
        }
        char u = read_key(0);
        if (u == 27) break;
        write_string("|\n");
    }
    return ret;
}

cmd_ret cmd_info(cmd_args args) {
    (void)args;
    cmd_ret ret;
    ret.errorcode = 0;
    file f = getFile(read_select);
    wsf("ssxsssd", "\nmode: 32bit", "\ncolor: ", VGA_COLOR, "\nselect: ", f.path, "\ndisk size: ", data_sectors_total);
    return ret;
}

cmd_ret cmd_strwrite(cmd_args args) {
    cmd_ret ret;
    ret.errorcode = 0;
	if(args.amount < 1) {
        RET_ERRCODE(3, "Insufficient arguments");
	}
    if (read_select == 0) {
		RET_ERRCODE(2, "No file selected");
    }
    for (int i = 0; i < 512; i++) sector_buffer[i] = 0;

    put_char('\n');
    get_string((char*)sector_buffer, 27, 512, 0b011);
    setFile(read_select, str_to_int(args.args[0]), sector_buffer);
    for (int i = 0; i < 512; i++) sector_buffer[i] = 0;
    return ret;
}

cmd_ret cmd_write(cmd_args args) {
    cmd_ret ret;
    ret.errorcode = 0;
    if(args.amount < 2) {
        RET_ERRCODE(3, "Insufficient arguments");
    }
    if (read_select == 0) {
        RET_ERRCODE(2, "No file selected");
    }
    for (int i = 0; i < 512; i++) sector_buffer[i] = 0;

    int pos = 0;
    for (int j = 1; j < args.amount && pos < 511; j++) {
        if (j > 1 && pos < 511) sector_buffer[pos++] = ' ';  // разделитель между склеенными аргументами
        int i = 0;
        while (args.args[j][i] && pos < 511) {
            sector_buffer[pos++] = args.args[j][i];
            i++;
        }
    }

    setFile(read_select, str_to_int(args.args[0]), sector_buffer);

    for (int k = 0; k < 512; k++) sector_buffer[k] = 0;
    return ret;
}

cmd_ret cmd_read(cmd_args args) {
    cmd_ret ret;
    ret.errorcode = 0;
	if(args.amount < 1) {
        RET_ERRCODE(3, "Insufficient arguments");
	}
    if (read_select == 0) {
		RET_ERRCODE(2, "No file selected");
    }
    for (int i = 0; i < 512; i++) sector_buffer[i] = 0;
    readFilePart(read_select, str_to_int(args.args[0]), sector_buffer);
    put_char('\n');
    for (int i = 0; i < 512; i++)
        if (sector_buffer[i]) put_char(sector_buffer[i]);
    return ret;
}

cmd_ret cmd_select(cmd_args args) {
    cmd_ret ret;
    ret.errorcode = 0;
	if(args.amount < 1) {
        RET_ERRCODE(3, "Insufficient arguments");
	}
    u32 mark = findFile(args.args[0]);
    if (mark == 0) {
		RET_ERRCODE(4, "File not found");
    }
    read_select = mark;
    return ret;
}

cmd_ret cmd_tab(cmd_args args) {
    cmd_ret ret;
    ret.errorcode = 0;
	if(args.amount < 1) {
        RET_ERRCODE(3, "Insufficient arguments");
	}
    tab_len = str_to_int(args.args[0]);
    return ret;
}

cmd_ret cmd_charmap(cmd_args args) {
    (void)args;
    cmd_ret ret;
    ret.errorcode = 0;
    for (int i = 10; i < 256; i++) wsf("ccdc", i, '|', i, newline);
    return ret;
}

cmd_ret cmd_scan_pci(cmd_args args) {
    (void)args;
    cmd_ret ret;
    ret.errorcode = 0;
    scan_pci();
    return ret;
}

cmd_ret cmd_panic(cmd_args args) {
    (void)args;
    cmd_ret ret;
    ret.errorcode = 0;
    Cpanic("err_generated_manually");
    return ret; // недостижимо
}

cmd_ret cmd_reboot(cmd_args args) {
    (void)args;
    cmd_ret ret;
    ret.errorcode = 0;
    outb(0x64, 0xFE);
    return ret;
}

cmd_ret cmd_touch(cmd_args args) {
    cmd_ret ret;
    ret.errorcode = 0;
	if(args.amount < 1) {
        RET_ERRCODE(3, "Insufficient arguments");
	}
    createFile(args.args[0]);
    return ret;
}

cmd_ret cmd_del(cmd_args args) {
    (void)args;
    cmd_ret ret;
    ret.errorcode = 0;
    if (read_select == 0) {
		RET_ERRCODE(2, "No file selected");
    }
    deleteFile(read_select);
    return ret;
}

cmd_ret cmd_dir(cmd_args args) {
    cmd_ret ret;
    ret.errorcode = 0;
    put_char(newline);
    if (args.amount >= 1) listFiles(write_string, args.args[0]);
    else listFiles(write_string, "/");
    return ret;
}

cmd_ret cmd_in(cmd_args args) {
    cmd_ret ret;
    ret.errorcode = 0;
	if(args.amount < 2) {
        RET_ERRCODE(3, "Insufficient arguments");
	}
    char mode = args.args[0][0];
    u16 port = str_to_int(args.args[1]);
    char buf[5];
    put_char(newline);
    if (mode == 'b') write_string(int_to_str(inb(port), buf, 5));
    if (mode == 'w') write_string(int_to_str(inw(port), buf, 5));
    if (mode == 'l') write_string(int_to_str(inl(port), buf, 5));
    return ret;
}

cmd_ret cmd_out(cmd_args args) {
    cmd_ret ret;
    ret.errorcode = 0;
	if(args.amount < 3) {
        RET_ERRCODE(3, "Insufficient arguments");
	}
    char mode = args.args[0][0];
    u16 port = str_to_int(args.args[1]);
    u32 val = str_to_int(args.args[2]);
    if (mode == 'b') outb(port, val);
    if (mode == 'w') outw(port, val);
    if (mode == 'l') outl(port, val);
    return ret;
}

cmd_ret cmd_time(cmd_args args) {
    (void)args;
    cmd_ret ret;
    ret.errorcode = 0;
    rtc_wait();
    u8 sec  = rtc_read(0x00);
    u8 min  = rtc_read(0x02);
    u8 hour = rtc_read(0x04);
    wsf("cdcdcd", newline, (utc + hour) % 24, ':', min, ':', sec);
    return ret;
}

cmd_ret cmd_date(cmd_args args) {
    (void)args;
    cmd_ret ret;
    ret.errorcode = 0;
    rtc_wait();
    u8 day = rtc_read(0x07);
    u8 mon = rtc_read(0x08);
    u16 year = rtc_read(0x09);
    wsf("cdcdcd", newline, day, '.', mon, space, 2000 + year);
    return ret;
}

cmd_ret cmd_utc(cmd_args args) {
    cmd_ret ret;
    ret.errorcode = 0;
	if(args.amount < 1) {
        RET_ERRCODE(3, "Insufficient arguments");
	}
    utc = str_to_int(args.args[0]);
    set_utc();
    return ret;
}

run_cmd commands[] = {
	{cmd_cls, "cls"},
    {cmd_color, "color"},
    {cmd_echo, "echo"},
    {cmd_delay, "delay"},
    {cmd_run, "run"},
    {cmd_dumb, "dumb"},
    {cmd_info, "info"},
    {cmd_strwrite, "strwrite"},
    {cmd_write, "write"},
    {cmd_read, "read"},
    {cmd_select, "select"},
    {cmd_tab, "tab"},
    {cmd_charmap, "charmap"},
    {cmd_scan_pci, "scan_pci"},
    {cmd_panic, "panic"},
    {cmd_reboot, "reboot"},
    {cmd_touch, "touch"},
    {cmd_del, "del"},
    {cmd_dir, "dir"},
    {cmd_in, "in"},
    {cmd_out, "out"},
    {cmd_time, "time"},
    {cmd_date, "date"},
    {cmd_utc, "utc"},
};