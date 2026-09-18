```mermaid
classDiagram
    Animal <|-- Duck
    Animal <|-- Fish
    Vehicle <|-- Car
    Vehicle <|-- Bus
    class Animal{
      +int age
    }
    class Vehicle{
      +int wheels
    }
```
