def locate_line_column(data, pos) :
    line = 1
    for s in data.splitlines() :
        if pos < len(s) :
            break
        pos -= len(s)
        line += 1
    column = pos + 1
    return line, column
