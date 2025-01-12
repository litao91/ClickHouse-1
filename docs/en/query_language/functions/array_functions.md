# Functions for working with arrays

## empty {#function-empty}

Returns 1 for an empty array, or 0 for a non-empty array.
The result type is UInt8.
The function also works for strings.

## notEmpty {#function-notempty}

Returns 0 for an empty array, or 1 for a non-empty array.
The result type is UInt8.
The function also works for strings.

## length {#array_functions-length}

Returns the number of items in the array.
The result type is UInt64.
The function also works for strings.

## emptyArrayUInt8, emptyArrayUInt16, emptyArrayUInt32, emptyArrayUInt64

## emptyArrayInt8, emptyArrayInt16, emptyArrayInt32, emptyArrayInt64

## emptyArrayFloat32, emptyArrayFloat64

## emptyArrayDate, emptyArrayDateTime

## emptyArrayString

Accepts zero arguments and returns an empty array of the appropriate type.

## emptyArrayToSingle

Accepts an empty array and returns a one-element array that is equal to the default value.

## range(end), range(start, end [, step])

Returns an array of numbers from start to end-1 by step.
If the argument `start` is not specified, defaults to 0.
If the argument `step` is not specified, defaults to 1.
It behaviors almost like pythonic `range`. But the difference is that all the arguments type must be `UInt` numbers.
Just in case, an exception is thrown if arrays with a total length of more than 100,000,000 elements are created in a data block.

## array(x1, ...), operator \[x1, ...\]

Creates an array from the function arguments.
The arguments must be constants and have types that have the smallest common type. At least one argument must be passed, because otherwise it isn't clear which type of array to create. That is, you can't use this function to create an empty array (to do that, use the 'emptyArray\*' function described above).
Returns an 'Array(T)' type result, where 'T' is the smallest common type out of the passed arguments.

## arrayConcat

Combines arrays passed as arguments.

```sql
arrayConcat(arrays)
```

**Parameters**

- `arrays` – Arbitrary number of arguments of [Array](../../data_types/array.md) type.
**Example**

```sql
SELECT arrayConcat([1, 2], [3, 4], [5, 6]) AS res
```
```text
┌─res───────────┐
│ [1,2,3,4,5,6] │
└───────────────┘
```

## arrayElement(arr, n), operator arr[n]

Get the element with the index `n` from the array `arr`. `n` must be any integer type.
Indexes in an array begin from one.
Negative indexes are supported. In this case, it selects the corresponding element numbered from the end. For example, `arr[-1]` is the last item in the array.

If the index falls outside of the bounds of an array, it returns some default value (0 for numbers, an empty string for strings, etc.), except for the case with a non-constant array and a constant index 0 (in this case there will be an error `Array indices are 1-based`).

## has(arr, elem)

Checks whether the 'arr' array has the 'elem' element.
Returns 0 if the the element is not in the array, or 1 if it is.

`NULL` is processed as a value.

```sql
SELECT has([1, 2, NULL], NULL)
```
```text
┌─has([1, 2, NULL], NULL)─┐
│                       1 │
└─────────────────────────┘
```

## hasAll

Checks whether one array is a subset of another.

```sql
hasAll(set, subset)
```

**Parameters**

- `set` – Array of any type with a set of elements.
- `subset` – Array of any type with elements that should be tested to be a subset of `set`.

**Return values**

- `1`, if `set` contains all of the elements from `subset`.
- `0`, otherwise.

**Peculiar properties**

- An empty array is a subset of any array.
- `Null` processed as a value.
- Order of values in both of arrays doesn't matter.

**Examples**

`SELECT hasAll([], [])` returns 1.

`SELECT hasAll([1, Null], [Null])` returns 1.

`SELECT hasAll([1.0, 2, 3, 4], [1, 3])` returns 1.

`SELECT hasAll(['a', 'b'], ['a'])` returns 1.

`SELECT hasAll([1], ['a'])` returns 0.

`SELECT hasAll([[1, 2], [3, 4]], [[1, 2], [3, 5]])` returns 0.

## hasAny

Checks whether two arrays have intersection by some elements.

```sql
hasAny(array1, array2)
```

**Parameters**

- `array1` – Array of any type with a set of elements.
- `array2` – Array of any type with a set of elements.

**Return values**

- `1`, if `array1` and `array2` have one similar element at least.
- `0`, otherwise.

**Peculiar properties**

- `Null` processed as a value.
- Order of values in both of arrays doesn't matter.

**Examples**

`SELECT hasAny([1], [])` returns `0`.

`SELECT hasAny([Null], [Null, 1])` returns `1`.

`SELECT hasAny([-128, 1., 512], [1])` returns `1`.

`SELECT hasAny([[1, 2], [3, 4]], ['a', 'c'])` returns `0`.

`SELECT hasAll([[1, 2], [3, 4]], [[1, 2], [1, 2]])` returns `1`.

## indexOf(arr, x)

Returns the index of the first 'x' element (starting from 1) if it is in the array, or 0 if it is not.

Example:

```sql
SELECT indexOf([1, 3, NULL, NULL], NULL)
```
```text

┌─indexOf([1, 3, NULL, NULL], NULL)─┐
│                                 3 │
└───────────────────────────────────┘
```

Elements set to `NULL` are handled as normal values.

## countEqual(arr, x)

Returns the number of elements in the array equal to x. Equivalent to arrayCount (elem -> elem = x, arr).

`NULL` elements are handled as separate values.

Example:

```sql
SELECT countEqual([1, 2, NULL, NULL], NULL)
```
```text
┌─countEqual([1, 2, NULL, NULL], NULL)─┐
│                                    2 │
└──────────────────────────────────────┘
```

## arrayEnumerate(arr) {#array_functions-arrayenumerate}

Returns the array \[1, 2, 3, ..., length (arr) \]

This function is normally used with ARRAY JOIN. It allows counting something just once for each array after applying ARRAY JOIN. Example:

```sql
SELECT
    count() AS Reaches,
    countIf(num = 1) AS Hits
FROM test.hits
ARRAY JOIN
    GoalsReached,
    arrayEnumerate(GoalsReached) AS num
WHERE CounterID = 160656
LIMIT 10
```
```text
┌─Reaches─┬──Hits─┐
│   95606 │ 31406 │
└─────────┴───────┘
```

In this example, Reaches is the number of conversions (the strings received after applying ARRAY JOIN), and Hits is the number of pageviews (strings before ARRAY JOIN). In this particular case, you can get the same result in an easier way:

```sql
SELECT
    sum(length(GoalsReached)) AS Reaches,
    count() AS Hits
FROM test.hits
WHERE (CounterID = 160656) AND notEmpty(GoalsReached)
```
```text
┌─Reaches─┬──Hits─┐
│   95606 │ 31406 │
└─────────┴───────┘
```

This function can also be used in higher-order functions. For example, you can use it to get array indexes for elements that match a condition.

## arrayEnumerateUniq(arr, ...)

Returns an array the same size as the source array, indicating for each element what its position is among elements with the same value.
For example: arrayEnumerateUniq(\[10, 20, 10, 30\]) = \[1, 1, 2, 1\].

This function is useful when using ARRAY JOIN and aggregation of array elements.
Example:

```sql
SELECT
    Goals.ID AS GoalID,
    sum(Sign) AS Reaches,
    sumIf(Sign, num = 1) AS Visits
FROM test.visits
ARRAY JOIN
    Goals,
    arrayEnumerateUniq(Goals.ID) AS num
WHERE CounterID = 160656
GROUP BY GoalID
ORDER BY Reaches DESC
LIMIT 10
```
```text
┌──GoalID─┬─Reaches─┬─Visits─┐
│   53225 │    3214 │   1097 │
│ 2825062 │    3188 │   1097 │
│   56600 │    2803 │    488 │
│ 1989037 │    2401 │    365 │
│ 2830064 │    2396 │    910 │
│ 1113562 │    2372 │    373 │
│ 3270895 │    2262 │    812 │
│ 1084657 │    2262 │    345 │
│   56599 │    2260 │    799 │
│ 3271094 │    2256 │    812 │
└─────────┴─────────┴────────┘
```

In this example, each goal ID has a calculation of the number of conversions (each element in the Goals nested data structure is a goal that was reached, which we refer to as a conversion) and the number of sessions. Without ARRAY JOIN, we would have counted the number of sessions as sum(Sign). But in this particular case, the rows were multiplied by the nested Goals structure, so in order to count each session one time after this, we apply a condition to the value of the arrayEnumerateUniq(Goals.ID) function.

The arrayEnumerateUniq function can take multiple arrays of the same size as arguments. In this case, uniqueness is considered for tuples of elements in the same positions in all the arrays.

```sql
SELECT arrayEnumerateUniq([1, 1, 1, 2, 2, 2], [1, 1, 2, 1, 1, 2]) AS res
```
```text
┌─res───────────┐
│ [1,2,1,1,2,1] │
└───────────────┘
```

This is necessary when using ARRAY JOIN with a nested data structure and further aggregation across multiple elements in this structure.

## arrayPopBack

Removes the last item from the array.

```sql
arrayPopBack(array)
```

**Parameters**

- `array` – Array.

**Example**

```sql
SELECT arrayPopBack([1, 2, 3]) AS res
```
```text
┌─res───┐
│ [1,2] │
└───────┘
```

## arrayPopFront

Removes the first item from the array.

```sql
arrayPopFront(array)
```

**Parameters**

- `array` – Array.

**Example**

```sql
SELECT arrayPopFront([1, 2, 3]) AS res
```
```text
┌─res───┐
│ [2,3] │
└───────┘
```

## arrayPushBack

Adds one item to the end of the array.

```sql
arrayPushBack(array, single_value)
```

**Parameters**

- `array` – Array.
- `single_value` – A single value. Only numbers can be added to an array with numbers, and only strings can be added to an array of strings. When adding numbers, ClickHouse automatically sets the `single_value` type for the data type of the array. For more information about the types of data in ClickHouse, see "[Data types](../../data_types/index.md#data_types)". Can be `NULL`. The function adds a `NULL` element to an array, and the type of array elements converts to `Nullable`.

**Example**

```sql
SELECT arrayPushBack(['a'], 'b') AS res
```
```text
┌─res───────┐
│ ['a','b'] │
└───────────┘
```

## arrayPushFront

Adds one element to the beginning of the array.

```sql
arrayPushFront(array, single_value)
```

**Parameters**

- `array` – Array.
- `single_value` – A single value. Only numbers can be added to an array with numbers, and only strings can be added to an array of strings. When adding numbers, ClickHouse automatically sets the `single_value` type for the data type of the array. For more information about the types of data in ClickHouse, see "[Data types](../../data_types/index.md#data_types)". Can be `NULL`. The function adds a `NULL` element to an array, and the type of array elements converts to `Nullable`.

**Example**

```sql
SELECT arrayPushBack(['b'], 'a') AS res
```
```text
┌─res───────┐
│ ['a','b'] │
└───────────┘
```

## arrayResize

Changes the length of the array.

```sql
arrayResize(array, size[, extender])
```

**Parameters:**

- `array` — Array.
- `size` — Required length of the array.
    - If `size` is less than the original size of the array, the array is truncated from the right.
- If `size` is larger than the initial size of the array, the array is extended to the right with `extender` values or default values for the data type of the array items.
- `extender` — Value for extending an array. Can be `NULL`.

**Returned value:**

An array of length `size`.

**Examples of calls**

```sql
SELECT arrayResize([1], 3)
```
```text
┌─arrayResize([1], 3)─┐
│ [1,0,0]             │
└─────────────────────┘
```

```sql
SELECT arrayResize([1], 3, NULL)
```
```text
┌─arrayResize([1], 3, NULL)─┐
│ [1,NULL,NULL]             │
└───────────────────────────┘
```

## arraySlice

Returns a slice of the array.

```sql
arraySlice(array, offset[, length])
```

**Parameters**

- `array` –  Array of data.
- `offset` – Indent from the edge of the array. A positive value indicates an offset on the left, and a negative value is an indent on the right. Numbering of the array items begins with 1.
- `length` - The length of the required slice. If you specify a negative value, the function returns an open slice `[offset, array_length - length)`. If you omit the value, the function returns the slice `[offset, the_end_of_array]`.

**Example**

```sql
SELECT arraySlice([1, 2, NULL, 4, 5], 2, 3) AS res
```
```text
┌─res────────┐
│ [2,NULL,4] │
└────────────┘
```

Array elements set to `NULL` are handled as normal values.

## arraySort(\[func,\] arr, ...) {#array_functions-sort}

Sorts the elements of the `arr` array in ascending order. If the `func` function is specified, sorting order is determined by the result of the `func` function applied to the elements of the array. If `func` accepts multiple arguments, the `arraySort` function is passed several arrays that the arguments of `func` will correspond to. Detailed examples are shown at the end of `arraySort` description.

Example of integer values sorting:

```sql
SELECT arraySort([1, 3, 3, 0]);
```
```text
┌─arraySort([1, 3, 3, 0])─┐
│ [0,1,3,3]               │
└─────────────────────────┘
```

Example of string values sorting:

```sql
SELECT arraySort(['hello', 'world', '!']);
```
```text
┌─arraySort(['hello', 'world', '!'])─┐
│ ['!','hello','world']              │
└────────────────────────────────────┘
```

Consider the following sorting order for the `NULL`, `NaN` and `Inf` values:

```sql
SELECT arraySort([1, nan, 2, NULL, 3, nan, -4, NULL, inf, -inf]);
```
```text
┌─arraySort([1, nan, 2, NULL, 3, nan, -4, NULL, inf, -inf])─┐
│ [-inf,-4,1,2,3,inf,nan,nan,NULL,NULL]                     │
└───────────────────────────────────────────────────────────┘
```

- `-Inf` values are first in the array.
- `NULL` values are last in the array.
- `NaN` values are right before `NULL`.
- `Inf` values are right before `NaN`.

Note that `arraySort` is a [higher-order function](higher_order_functions.md). You can pass a lambda function to it as the first argument. In this case, sorting order is determined by the result of the lambda function applied to the elements of the array.

Let's consider the following example:

```sql
SELECT arraySort((x) -> -x, [1, 2, 3]) as res;
```
```text
┌─res─────┐
│ [3,2,1] │
└─────────┘
```

For each element of the source array, the lambda function returns the sorting key, that is, [1 –> -1, 2 –> -2, 3 –> -3]. Since the `arraySort` function sorts the keys in ascending order, the result is [3, 2, 1]. Thus, the `(x) –> -x` lambda function sets the [descending order](#array_functions-reverse-sort) in a sorting.

The lambda function can accept multiple arguments. In this case, you need to pass the `arraySort` function several arrays of identical length that the arguments of lambda function will correspond to. The resulting array will consist of elements from the first input array; elements from the next input array(s) specify the sorting keys. For example:

```sql
SELECT arraySort((x, y) -> y, ['hello', 'world'], [2, 1]) as res;
```
```text
┌─res────────────────┐
│ ['world', 'hello'] │
└────────────────────┘
```

Here, the elements that are passed in the second array ([2, 1]) define a sorting key for the corresponding element from the source array (['hello', 'world']), that is, ['hello' –> 2, 'world' –> 1]. Since the lambda function doesn't use `x`, actual values of the source array don't affect the order in the result. So, 'hello' will be the second element in the result, and 'world' will be the first.

Other examples are shown below.

```sql
SELECT arraySort((x, y) -> y, [0, 1, 2], ['c', 'b', 'a']) as res;
```
```text
┌─res─────┐
│ [2,1,0] │
└─────────┘
```

```sql
SELECT arraySort((x, y) -> -y, [0, 1, 2], [1, 2, 3]) as res;
```
```text
┌─res─────┐
│ [2,1,0] │
└─────────┘
```

!!! note
    To improve sorting efficiency, the [Schwartzian transform](https://en.wikipedia.org/wiki/Schwartzian_transform) is used.

## arrayReverseSort([func,] arr, ...) {#array_functions-reverse-sort}

Sorts the elements of the `arr` array in descending order. If the `func` function is specified, `arr` is sorted according to the result of the `func` function applied to the elements of the array, and then the sorted array is reversed. If `func` accepts multiple arguments, the `arrayReverseSort` function is passed several arrays that the arguments of `func` will correspond to. Detailed examples are shown at the end of `arrayReverseSort` description.

Example of integer values sorting:

```sql
SELECT arrayReverseSort([1, 3, 3, 0]);
```
```text
┌─arrayReverseSort([1, 3, 3, 0])─┐
│ [3,3,1,0]                      │
└────────────────────────────────┘
```

Example of string values sorting:

```sql
SELECT arrayReverseSort(['hello', 'world', '!']);
```
```text
┌─arrayReverseSort(['hello', 'world', '!'])─┐
│ ['world','hello','!']                     │
└───────────────────────────────────────────┘
```

Consider the following sorting order for the `NULL`, `NaN` and `Inf` values:

```sql
SELECT arrayReverseSort([1, nan, 2, NULL, 3, nan, -4, NULL, inf, -inf]) as res;
```
```text
┌─res───────────────────────────────────┐
│ [inf,3,2,1,-4,-inf,nan,nan,NULL,NULL] │
└───────────────────────────────────────┘
```

- `Inf` values are first in the array.
- `NULL` values are last in the array.
- `NaN` values are right before `NULL`.
- `-Inf` values are right before `NaN`.

Note that the `arrayReverseSort` is a [higher-order function](higher_order_functions.md). You can pass a lambda function to it as the first argument. Example is shown below.

```sql
SELECT arrayReverseSort((x) -> -x, [1, 2, 3]) as res;
```
```text
┌─res─────┐
│ [1,2,3] │
└─────────┘
```

The array is sorted in the following way:

1. At first, the source array ([1, 2, 3]) is sorted according to the result of the lambda function applied to the elements of the array. The result is an array [3, 2, 1].
2. Array that is obtained on the previous step, is reversed. So, the final result is [1, 2, 3].

The lambda function can accept multiple arguments. In this case, you need to pass the `arrayReverseSort` function several arrays of identical length that the arguments of lambda function will correspond to. The resulting array will consist of elements from the first input array; elements from the next input array(s) specify the sorting keys. For example:

```sql
SELECT arrayReverseSort((x, y) -> y, ['hello', 'world'], [2, 1]) as res;
```
```text
┌─res───────────────┐
│ ['hello','world'] │
└───────────────────┘
```

In this example, the array is sorted in the following way:

1. At first, the source array (['hello', 'world']) is sorted according to the result of the lambda function applied to the elements of the arrays. The elements that are passed in the second array ([2, 1]), define the sorting keys for corresponding elements from the source array. The result is an array ['world', 'hello'].
2. Array that was sorted on the previous step, is reversed. So, the final result is ['hello', 'world'].

Other examples are shown below.

```sql
SELECT arrayReverseSort((x, y) -> y, [4, 3, 5], ['a', 'b', 'c']) AS res;
```
```text
┌─res─────┐
│ [5,3,4] │
└─────────┘
```
```sql
SELECT arrayReverseSort((x, y) -> -y, [4, 3, 5], [1, 2, 3]) AS res;
```
```text
┌─res─────┐
│ [4,3,5] │
└─────────┘
```

## arrayUniq(arr, ...)

If one argument is passed, it counts the number of different elements in the array.
If multiple arguments are passed, it counts the number of different tuples of elements at corresponding positions in multiple arrays.

If you want to get a list of unique items in an array, you can use arrayReduce('groupUniqArray', arr).

## arrayJoin(arr) {#array_functions-join}

A special function. See the section ["ArrayJoin function"](array_join.md#functions_arrayjoin).

## arrayDifference(arr) {#array_functions-arraydifference}

Takes an array, returns an array of differences between adjacent elements. The first element will be 0, the second is the difference between the second and first elements of the original array, etc. The type of elements in the resulting array is determined by the type inference rules for subtraction (e.g. UInt8 - UInt8 = Int16). UInt*/Int*/Float* types are supported (type Decimal is not supported).

Example:

```sql
SELECT arrayDifference([1, 2, 3, 4])
```

```text
┌─arrayDifference([1, 2, 3, 4])─┐
│ [0,1,1,1]                     │
└───────────────────────────────┘
```

Example of the overflow due to result type Int64:

```sql
SELECT arrayDifference([0, 10000000000000000000])
```

```text
┌─arrayDifference([0, 10000000000000000000])─┐
│ [0,-8446744073709551616]                   │
└────────────────────────────────────────────┘
```

## arrayDistinct(arr) {#array_functions-arraydistinct}

Takes an array, returns an array containing the distinct elements.

Example:

```sql
SELECT arrayDistinct([1, 2, 2, 3, 1])
```

```text
┌─arrayDistinct([1, 2, 2, 3, 1])─┐
│ [1,2,3]                        │
└────────────────────────────────┘
```

## arrayEnumerateDense(arr) {#array_functions-arrayenumeratedense}

Returns an array of the same size as the source array, indicating where each element first appears in the source array.

Example:

```sql
SELECT arrayEnumerateDense([10, 20, 10, 30])
```

```text
┌─arrayEnumerateDense([10, 20, 10, 30])─┐
│ [1,2,1,3]                             │
└───────────────────────────────────────┘
```

## arrayIntersect(arr) {#array_functions-arrayintersect}

Takes multiple arrays, returns an array with elements that are present in all source arrays. Elements order in the resulting array is the same as in the first array.

Example:

```sql
SELECT
    arrayIntersect([1, 2], [1, 3], [2, 3]) AS no_intersect,
    arrayIntersect([1, 2], [1, 3], [1, 4]) AS intersect
```

```text
┌─no_intersect─┬─intersect─┐
│ []           │ [1]       │
└──────────────┴───────────┘
```

## arrayReduce(agg_func, arr1, ...) {#array_functions-arrayreduce}

Applies an aggregate function to array elements and returns its result. The name of the aggregation function is passed as a string in single quotes `'max'`, `'sum'`. When using parametric aggregate functions, the parameter is indicated after the function name in parentheses `'uniqUpTo(6)'`.

Example:

```sql
SELECT arrayReduce('max', [1, 2, 3])
```

```text
┌─arrayReduce('max', [1, 2, 3])─┐
│                             3 │
└───────────────────────────────┘
```

If an aggregate function takes multiple arguments, then this function must be applied to multiple arrays of the same size.

Example:

```sql
SELECT arrayReduce('maxIf', [3, 5], [1, 0])
```

```text
┌─arrayReduce('maxIf', [3, 5], [1, 0])─┐
│                                    3 │
└──────────────────────────────────────┘
```

Example with a parametric aggregate function:

```sql
SELECT arrayReduce('uniqUpTo(3)', [1, 2, 3, 4, 5, 6, 7, 8, 9, 10])
```

```text
┌─arrayReduce('uniqUpTo(3)', [1, 2, 3, 4, 5, 6, 7, 8, 9, 10])─┐
│                                                           4 │
└─────────────────────────────────────────────────────────────┘
```

## arrayFlatten(arr) {#array_functions-arrayflatten}

The `arrayFlatten` (or `flatten` alias) method will collapse the elements of an array to create a single array.

Example:

```sql
SELECT arrayFlatten([[1, 2, 3], [4, 5]])
```

```text
┌─arrayFlatten([[1, 2, 3], [4, 5]])─┐
│                       [1,2,3,4,5] │
└───────────────────────────────────┘
```

## arrayReverse(arr) {#array_functions-arrayreverse}

Returns an array of the same size as the original array containing the elements in reverse order.

Example:

```sql
SELECT arrayReverse([1, 2, 3])
```

```text
┌─arrayReverse([1, 2, 3])─┐
│ [3,2,1]                 │
└─────────────────────────┘
```

## reverse(arr) {#array_functions-reverse}

Synonym for ["arrayReverse"](#array_functions-arrayreverse)

## arrayCompact {#arraycompact}

Removes consecutive duplicate elements from an array. The order of result values is determined by the order in the source array.

**Syntax**

```sql
arrayCompact(arr)
```

**Parameters**

`arr` — The [array](../../data_types/array.md) to inspect.

**Returned value**

The array without duplicate.

Type: `Array`.

**Example**

Query:

```sql
SELECT arrayCompact([1, 1, nan, nan, 2, 3, 3, 3])
```

Result:

```text
┌─arrayCompact([1, 1, nan, nan, 2, 3, 3, 3])─┐
│ [1,nan,nan,2,3]                            │
└────────────────────────────────────────────┘
```

[Original article](https://clickhouse.yandex/docs/en/query_language/functions/array_functions/) <!--hide-->