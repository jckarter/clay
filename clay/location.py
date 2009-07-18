class Location(object) :
    def __init__(self, data, start, end, file_name) :
        self.data = data
        self.start = start
        self.end = end
        self.file_name = file_name

    def new_start(self, start) :
        return Location(self.data, start, self.end, self.file_name)

    def new_end(self, end) :
        return Location(self.data, self.start, end, self.file_name)
