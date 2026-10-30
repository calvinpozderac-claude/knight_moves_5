from st import *
s = State()
path = static_path(s, ix('g7'))
print('path a1->g7:', [nm(i) for i in path])
s = walk(s, path)
s.show()
print(s.options())
