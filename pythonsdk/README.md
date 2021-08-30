# Blue Brain CircuitExplorer Python SDK

The CircuitExplorer package provides an extended python API for the Blue Brain CircuitExplorer application

## Installation

### 1. From the Python Package Index

```
(venv)$ pip install circuitexplorer
```

### 2. From source

Clone the repository and install it:

```
(venv)$ git clone https://github.com/favreau/CircuitExplorer.git
(venv)$ cd CircuitExplorer/circuitexplorer/pythonsdk
(venv)$ python setup.py install
```

## API

### Connect to running Blue Brain CircuitExplorer instance

```python
>>> from circuitexplorer import CircuitExplorer
>>> bio_explorer = CircuitExplorer('localhost:8200')
```

# Upload to pypi

```bash
twine upload dist/*
```
